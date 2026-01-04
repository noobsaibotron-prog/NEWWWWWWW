# Changelog

All notable changes to AI Equalizer Pro will be documented in this file.

## [2.1.1] - 2025-01-XX

### Improved
- **Build System**: Fixed Visual Studio generator detection in BUILD_FINAL.bat
- **CMake**: Modernized CMakeLists.txt with better compiler options and output directories
- **Performance**: Added denormal handling in DSP processing to prevent CPU spikes
- **Code Quality**: Improved error handling and safety checks throughout
- **Documentation**: Added .clang-format and .gitignore files

### Fixed
- Fixed incorrect Visual Studio 18 2026 reference (now uses 17 2022 or 19 2024)
- Improved auto-gain calculation with better constants and safety checks
- Enhanced spectrum analyzer with denormal flushing
- Better thread safety in AI problem detection

### Technical
- Added constexpr constants for magic numbers
- Improved compiler warnings and optimization flags
- Better memory safety with bounds checking
- Enhanced code formatting standards

## [2.1.0] - 2024-11-XX

### Added
- Source Profiles (Generic, Vocals, Drums, Bass, Synth, Master, EDM)
- A/B Comparison with memory slots
- Auto-Gain compensation
- Pre/Post EQ spectrum overlay
- Peak Hold on analyzer
- Analysis History (last 5 snapshots)
- Thread safety improvements

### Changed
- Improved AI detection algorithms
- Better genre detection
- Enhanced GUI responsiveness

## [2.0.0] - Initial Release

### Added
- 8-band parametric EQ
- Real-time AI analysis
- Reference track matching
- User learning system
- Modern GUI design

