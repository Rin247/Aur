# HANDOVER

Graphics policy (USAGE BAN) handover: a temporary DX11/D3D11 solution is not acceptable.
Revert any active D3D11 backend use; rendering must run on Vulkan or D3D12 only.
Route backend selection through one shim; startup assert backend in {Vulkan, D3D12};
headless Null only under RHI_POLICY_HEADLESS=1. See C:\Projects\POLICY.md.
