# Adding features

1. Create `src/features/YourFeature.cpp` with a class inheriting `sfc::IFeature`.
2. Export `std::unique_ptr<IFeature> CreateYourFeature()`.
3. Register it in `src/features/RegisterFeatures.cpp`.
4. Add the `.cpp` to `CMakeLists.txt`.
5. Prefer `ConsoleBridge` for game changes; do not invent unverified engine APIs.
6. Add `CollectSearch` entries for command palette discoverability.
7. Rebuild Win32 Release and smoke-test in-game.

## UI guidelines

- Use `ImGui::SeparatorText` for sections
- Unique `##ids` on controls
- Amber terminal theme via `Theme::Apply`
- Never block the whole screen with opaque panels
- Destructive actions need confirmation
