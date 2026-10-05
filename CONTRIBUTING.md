# Contributing to Turbocharged

Bug reports and focused pull requests are welcome. Development targets Windows
and Linux; other platforms and online functionality are not currently supported.

## Report a problem

Use the GitHub bug-report form. Include the version/commit, operating system,
GPU/driver, relevant settings, reproduction steps, and logs. Mention mods and
whether the problem also occurs with a fresh configuration. Back up your saves
before experimenting with a different build or configuration.

## Build and change the code

Follow [Building from source](README.md#building-from-source). The native build
requires a 32-bit target; use the supplied CMake presets. Keep pull requests
focused and describe the resulting behavior and how you checked it.

Use the clang-format version in `.clang-format-version`. Changed lines must
follow the repository's style; avoid reformatting unrelated upstream code.
`git config core.hooksPath .githooks` enables the local formatting hook.

Run the tests relevant to your change with `ctest --test-dir <build>
--output-on-failure`. Renderer integration tests require
`-DCTR_NATIVE_RENDERER_TESTS=ON`; the `gpu` subset needs an OpenGL context.
On Linux CI it runs with Mesa under Xvfb. Explain skipped tests or checks you
could not run. Add a regression test when it meaningfully catches the problem.

## Assets and licences

Keep retail disc data, extracted assets, player saves, account keys, and
third-party fonts outside commits. Any deliberately distributed asset or
dependency needs documented provenance and the required licence text in
`THIRD_PARTY_NOTICES.md` and `licenses/`. Packaging uses an explicit allowlist.
AppImage releases also include their runtime source archive and its checksum.
