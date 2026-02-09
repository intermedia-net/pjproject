// Empty wrapper target for SwiftPM.
// The binary module is still imported as `import PJSipIOS` by consumers.

// DO NOT DELETE: because of current technical restrictions of how SPM is working,
// this file is needed to be able to export binaries
// with external dependencies, linker settings, etc...

public enum PJSipIOSSPM {}
