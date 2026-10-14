# Windows regression checks

The runtime suite runs 21 cases against the current dock code with real Qt and
libobs settings/file handling. It covers settings recovery and preservation,
replacement sources, collection-event delivery, DST gaps/folds, and conflicting
export paths. The source registry, frontend boundary and locale lookup are
simulated. A generated build copy exposes private members and injects fixed test
times; production files are not changed.

After configuring the normal Windows build dependencies:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/runtime.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/package.ps1
```

`runtime.ps1` accepts `-DependenciesRoot` to reuse an existing OBS dependency
directory. It requires Visual Studio 2022, CMake, Qt 6 and the development OBS
dependencies. Outputs go to ignored `build-tests/`.

Malformed-JSON diagnostics on stderr are expected in corruption fixtures.
The packaging check simulates only CMake failure and verifies the real script's
working directory, failure propagation and caller-directory restoration without
building, exporting or deploying files. These checks do not replace live OBS
load/output/unload testing or testing on other operating systems.
