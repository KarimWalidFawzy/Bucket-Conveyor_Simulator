# Bucket-Conveyor Simulator

A Qt Widgets application that simulates three camera stations inspecting balls as they move through an eight-slot bucket conveyor. Each station renders a synthetic conveyor frame, extracts ball features, tracks each ball across the conveyor, and emits a `PASS` or `REJECT` verdict when the track exits the final slot.

## Features

- Three conveyor stations running in separate Qt worker threads.
- Eight bucket slots per station, with one new slot spawned on every trigger.
- Random ball generation with four base colors.
- Synthetic defects represented by black patches covering 90 or 180 degrees of a ball.
- Automatic station pipelines with staggered speeds: 120 ms, 145 ms, and 170 ms.
- Frame-based feature extraction for ball color, radius, and defect area.
- Temporal tracking across a ball's eight-slot lifespan.
- Majority-vote defect classification with a confidence score.
- Live rendering of Station 0 and verdict updates in the status bar.
- Manual `Trigger` button to advance all stations once in addition to their timers.

## Requirements

- CMake 3.16 or newer
- A C++17 compiler
- Qt 5 or Qt 6 with the `Widgets` component

The project is configured for Qt's automatic MOC, UIC, and RCC processing.

## Build

From the project directory, configure and build with CMake:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

If CMake cannot find Qt, provide the Qt installation prefix through `CMAKE_PREFIX_PATH`. For example:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.9.0\mingw_64"
cmake --build build --config Release
```

The executable is produced in the build directory. With a multi-configuration generator on Windows, it is normally located at:

```text
build\Release\Bucket-Conveyor_Simulator.exe
```

With a single-configuration generator, use the executable path reported by CMake, commonly:

```text
build\Bucket-Conveyor_Simulator.exe
```

## Running

Start the executable. The three station pipelines begin automatically when the main window opens.

- The conveyor visualization is shown in the main display area.
- Click `Trigger` to enqueue one trigger for every station.
- The status bar reports trigger activity and finalized ball verdicts.
- `PASS` means the fused observations did not meet the defect-vote threshold; `REJECT` means at least half of the observations reported a defect.

## How It Works

1. `ConveyorManager` creates one `ConveyorStation`, `StationWorker`, and `QThread` for each station.
2. Each `ConveyorStation` advances existing balls, shifts the eight bucket slots, spawns a new slot, and emits a rendered `QImage`.
3. The corresponding `StationWorker` divides the frame into eight regions of interest and measures colored pixels and dark defect pixels.
4. Observations are associated with tracks as balls move from slot 0 through slot 7.
5. When a track leaves slot 7, the worker averages its measurements and emits a `BallFusedVerdict`.

By default, a new bucket contains a ball 70% of the time. A generated ball has a defect 40% of the time, and defective balls receive a randomly positioned 90-degree or 180-degree black patch. These values are simulation parameters, not configurable UI settings.

## Project Structure

| File | Purpose |
| --- | --- |
| `main.cpp` | Starts the Qt application and main window |
| `mainwindow.*` | Main window, visualization, button handling, and status messages |
| `conveyormanager.h` | Creates and coordinates station pipelines and threads |
| `conveyorstation.*` | Advances buckets, generates balls, and renders frames |
| `stationworker.*` | Extracts observations, tracks balls, and fuses verdicts |
| `ConveyorTypes.h` | Shared simulation, observation, tracking, and verdict data types |
| `mainwindow.ui` | Qt Designer layout for the main window |
| `CMakeLists.txt` | Qt/CMake build configuration |

## Limitations

- This is a synthetic simulation; it does not connect to physical cameras or conveyor hardware.
- Station 0's rendered frame is the only frame displayed in the main window, although all three stations process data.
- There are currently no automated tests or configurable runtime parameters.
