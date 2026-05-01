-- EML compatibility shim.
--
-- Embervale Minimap is a native Shroudtopia mod loaded through
-- CreateModInstance from minimap_mod.dll. This no-op Lua file exists only so
-- EML-based setups that scan every folder under mods/ can treat this folder as
-- a harmless Lua mod instead of an incomplete EML package.
return {}
