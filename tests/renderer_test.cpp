// Exercise the production submission path with delayed GPU completion.
#include "../src/dllmain.cpp"
#include <iostream>

namespace
{
    constexpr std::int32_t TEST_ERROR = -4;
    struct FakeGpu
    {
        bool pending[2] = {};
        bool signaled[2] = { true, true };
        bool invalidReset = false, invalidSubmit = false, idle = false, destroyedBeforeIdle = false;
        int commandResets = 0, fenceResets = 0, submissions = 0, destroyedFences = 0;
        bool recordFault = false, submitFault = false, faultScopeUnwound = false;
        std::int32_t statusResult = 0, commandResetResult = 0, beginResult = 0, endResult = 0, fenceResetResult = 0, submitResult = 0;
    } gpu;
    struct FaultScope { ~FaultScope() { gpu.faultScopeUnwound = true; } };
    const void* gameSemaphore = reinterpret_cast<void*>(0x500);
    VkPresentInfoKHR present{}, adjusted{};
    const void* adjustedSemaphore = nullptr;

    void Check(bool ok, const char* description)
    {
        if (!ok)
        {
            std::cerr << "FAIL: " << description << '\n';
            std::exit(1);
        }
        std::cout << "PASS: " << description << '\n';
    }

    void Complete(std::size_t index)
    {
        gpu.pending[index] = false;
        gpu.signaled[index] = true;
    }

    void Setup()
    {
        gpu = {};
        g_renderer = {};
        g_renderer.ready = true;
        g_renderer.device = 0x1;
        g_renderer.commandBuffers = { 0x100, 0x101 };
        g_renderer.framebuffers = { 0x200, 0x201 };
        g_renderer.submissionFences = { 0x300, 0x301 };
        g_renderer.renderCompleteSemaphores = { 0x400, 0x401 };
        g_renderer.fns.getFenceStatus = [](void*, void* fence) -> std::int32_t {
            if (gpu.statusResult != VK_SUCCESS) return gpu.statusResult;
            return gpu.signaled[reinterpret_cast<uintptr_t>(fence) - 0x300] ? VK_SUCCESS : VK_NOT_READY;
        };
        g_renderer.fns.resetCommandBuffer = [](void* command, std::uint32_t) -> std::int32_t {
            ++gpu.commandResets;
            gpu.invalidReset |= gpu.pending[reinterpret_cast<uintptr_t>(command) - 0x100];
            return gpu.commandResetResult;
        };
        g_renderer.fns.beginCommandBuffer = [](void*, const VkCommandBufferBeginInfo*) -> std::int32_t { return gpu.beginResult; };
        g_renderer.fns.endCommandBuffer = [](void*) -> std::int32_t { return gpu.endResult; };
        g_renderer.fns.cmdBeginRenderPass = [](void*, const VkRenderPassBeginInfo*, std::uint32_t) {
            if (gpu.recordFault) { FaultScope scope; RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr); }
        };
        g_renderer.fns.cmdEndRenderPass = [](void*) {};
        g_renderer.fns.resetFences = [](void*, std::uint32_t count, void* const* fences) -> std::int32_t {
            ++gpu.fenceResets;
            const auto index = reinterpret_cast<uintptr_t>(fences[0]) - 0x300;
            gpu.invalidReset |= count != 1 || gpu.pending[index];
            if (gpu.fenceResetResult == VK_SUCCESS) gpu.signaled[index] = false;
            return gpu.fenceResetResult;
        };
        g_renderer.fns.queueSubmit = [](void*, std::uint32_t count, const VkSubmitInfo* info, void* fence) -> std::int32_t {
            ++gpu.submissions;
            if (gpu.submitFault) { FaultScope scope; RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr); }
            const auto index = reinterpret_cast<uintptr_t>(fence) - 0x300;
            if (index >= 2) { gpu.invalidSubmit = true; return TEST_ERROR; }
            gpu.invalidSubmit |= count != 1 || gpu.signaled[index] || gpu.pending[index] ||
                info->commandBufferCount != 1 || info->signalSemaphoreCount != 1 ||
                reinterpret_cast<uintptr_t>(info->pCommandBuffers[0]) != 0x100 + index ||
                reinterpret_cast<uintptr_t>(info->pSignalSemaphores[0]) != 0x400 + index ||
                info->waitSemaphoreCount != present.waitSemaphoreCount || info->pWaitSemaphores != present.pWaitSemaphores ||
                (info->waitSemaphoreCount != 0 && (info->pWaitDstStageMask == nullptr ||
                    info->pWaitDstStageMask[0] != VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT));
            if (gpu.submitResult == VK_SUCCESS) gpu.pending[index] = true;
            return gpu.submitResult;
        };
        g_renderer.fns.deviceWaitIdle = [](void*) -> std::int32_t {
            gpu.idle = true;
            Complete(0);
            Complete(1);
            return VK_SUCCESS;
        };
        g_renderer.fns.destroyFence = [](void*, void*, const void*) {
            ++gpu.destroyedFences;
            gpu.destroyedBeforeIdle |= !gpu.idle;
        };
        present = {};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &gameSemaphore;
        adjusted = {};
        adjusted.waitSemaphoreCount = 7; // Untouched unless an overlay was submitted.
        adjustedSemaphore = nullptr;
    }

    bool Submit(std::uint32_t index = 0)
    {
        return SubmitVulkanMinimapFrameLocked(reinterpret_cast<void*>(0x2), present, index, adjusted, &adjustedSemaphore);
    }
}

int main()
{
    // Same handle and extent do not establish that cached image views are valid.
    Setup();
    SwapchainRuntimeInfo snapshot{};
    snapshot.device = g_renderer.device;
    snapshot.handle = g_renderer.swapchain = 0x900;
    snapshot.width = g_renderer.width = 1920;
    snapshot.height = g_renderer.height = 1080;
    snapshot.format = g_renderer.format = 37;
    snapshot.images = g_renderer.images = { 0x910, 0x911 };
    snapshot.generation = g_renderer.swapchainGeneration = 1;
    Check(BuildVulkanMinimapRendererLocked(snapshot), "unchanged swapchain reuses the renderer");
    snapshot.images[0] = 0x912;
    Check(!RendererMatchesSwapchain(g_renderer, snapshot, 1920, 1080, 37), "changed image handles invalidate same-handle renderer");
    snapshot.images.clear();
    Check(RendererMatchesSwapchain(g_renderer, snapshot, 1920, 1080, 37), "missing image list alone does not rebuild each frame");
    ++snapshot.generation;
    Check(!RendererMatchesSwapchain(g_renderer, snapshot, 1920, 1080, 37), "known recreation invalidates renderer before image list arrives");
    VkSwapchainCreateInfoKHR create{};
    create.imageFormat = 37;
    create.imageExtent = { 1920, 1080 };
    RememberSwapchainCreate(reinterpret_cast<void*>(1), &create, reinterpret_cast<void*>(0x900), VK_SUCCESS);
    SwapchainRuntimeInfo first{}, second{};
    TryGetSwapchainSnapshot(0x900, first);
    RememberSwapchainCreate(reinterpret_cast<void*>(1), &create, reinterpret_cast<void*>(0x900), VK_SUCCESS);
    TryGetSwapchainSnapshot(0x900, second);
    Check(second.generation > first.generation && second.images.empty(), "same-handle creations have distinct generations");

    Setup();
    Check(Submit() && !gpu.invalidSubmit && !gpu.invalidReset && gpu.pending[0], "first frame submits with its completion fence");
    Check(adjusted.waitSemaphoreCount == 1 && adjusted.pWaitSemaphores == &adjustedSemaphore &&
        adjustedSemaphore == reinterpret_cast<void*>(0x400), "successful overlay updates the presentation wait");
    adjusted.waitSemaphoreCount = 7;
    adjustedSemaphore = nullptr;
    Check(!Submit() && gpu.commandResets == 1 && gpu.fenceResets == 1 && gpu.submissions == 1 &&
        !gpu.invalidReset && !g_renderer.submissionFailed, "busy GPU skips drawing without resetting or resubmitting pending work");
    Check(adjusted.waitSemaphoreCount == 7 && adjustedSemaphore == nullptr && present.pWaitSemaphores == &gameSemaphore,
        "skipped overlay leaves the game's presentation wait unchanged");
    Check(Submit(1) && gpu.pending[0] && gpu.pending[1] && !gpu.invalidSubmit, "swapchain images have independent completion tracking");
    Complete(0);
    Check(Submit() && gpu.submissions == 3 && !gpu.invalidReset, "completed image can be recorded and submitted again");
    for (std::uint32_t frame = 0; frame < 100000; ++frame)
    {
        const auto index = frame % 2;
        if (frame % 3 == 0) Complete(index);
        const bool expected = gpu.signaled[index];
        if (Submit(index) != expected || gpu.invalidReset || gpu.invalidSubmit)
            Check(false, "delayed completion stress sequence");
    }
    Check(true, "100000 submissions/skips with delayed GPU completion never reset pending work");
    DestroyVulkanMinimapRendererLocked();
    Check(gpu.destroyedFences == 2 && !gpu.destroyedBeforeIdle && !g_renderer.ready && g_renderer.submissionFences.empty(),
        "teardown waits before destroying fences and clearing state");

    Setup();
    Check(!Submit(2) && gpu.commandResets == 0 && gpu.submissions == 0, "invalid swapchain image is rejected");
    present.waitSemaphoreCount = 0;
    present.pWaitSemaphores = nullptr;
    Check(Submit() && !gpu.invalidSubmit, "presentation without game wait semaphores is supported");

    Setup();
    gpu.statusResult = TEST_ERROR;
    Check(!Submit() && g_renderer.submissionFailed && gpu.commandResets == 0 && gpu.submissions == 0,
        "fence query failure stops drawing before modifying resources");
    gpu.statusResult = VK_SUCCESS;
    Check(!Submit() && gpu.commandResets == 0, "drawing stays stopped after a synchronization failure");
    for (int stage = 0; stage < 3; ++stage)
    {
        Setup();
        if (stage == 0) gpu.commandResetResult = TEST_ERROR;
        if (stage == 1) gpu.beginResult = TEST_ERROR;
        if (stage == 2) gpu.endResult = TEST_ERROR;
        Check(!Submit() && g_renderer.submissionFailed && gpu.fenceResets == 0 && gpu.submissions == 0 &&
            adjusted.waitSemaphoreCount == 7, "recording failure preserves the signaled fence and original presentation");
    }
    Setup();
    gpu.fenceResetResult = TEST_ERROR;
    Check(!Submit() && g_renderer.submissionFailed && gpu.submissions == 0 && adjusted.waitSemaphoreCount == 7,
        "fence reset failure prevents queue submission");
    Setup();
    gpu.submitResult = TEST_ERROR;
    Check(!Submit() && g_renderer.submissionFailed && adjusted.waitSemaphoreCount == 7 && adjustedSemaphore == nullptr,
        "failed submission does not make presentation wait on an unsignaled overlay semaphore");
    gpu.submitResult = VK_SUCCESS;
    Check(!Submit() && gpu.submissions == 1 && gpu.commandResets == 1, "failed submission is not retried with uncertain state");
    Setup();
    gpu.recordFault = true;
    Check(!Submit() && g_renderer.submissionFailed && gpu.faultScopeUnwound &&
        gpu.fenceResets == 0 && gpu.submissions == 0 && adjusted.waitSemaphoreCount == 7,
        "recording access violation unwinds scopes and stops drawing without changing presentation");
    Setup();
    gpu.submitFault = true;
    Check(!Submit() && g_renderer.submissionFailed && gpu.faultScopeUnwound && adjusted.waitSemaphoreCount == 7,
        "submit access violation unwinds scopes and preserves original presentation waits");
    gpu.submitFault = false;
    Check(!Submit() && gpu.submissions == 1, "driver access violation is not retried on the same renderer");
    Setup();
    g_renderer.submissionFences = { 0x300, 0 };
    g_renderer.submissionFailed = true;
    DestroyVulkanMinimapRendererLocked();
    Check(gpu.destroyedFences == 1 && !gpu.destroyedBeforeIdle && !g_renderer.submissionFailed,
        "partial fence allocation cleans up valid handles and clears failure state");
    return 0;
}
