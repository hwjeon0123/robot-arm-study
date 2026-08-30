[🇰🇷 한국어](README.md) | 🇺🇸 English

# robot-arm-study

A learning log on building a robot-arm pick-and-place system with ROS 2 Jazzy + Gazebo, and
advancing it with vision and AI.
Built on a Podman container environment on headless Ubuntu, using an Nvidia graphics card's
3D hardware acceleration.
Since this is used over a remote connection, `vglrun` is set up so 3D acceleration is
available inside the container as well.

Remote access uses **Sunshine**. xrdp or VNC can't get GPU acceleration through, so Gazebo
or RViz are effectively unusable over them. Sunshine spins up a virtual monitor with Xorg
and streams the screen encoded on the GPU, so 3D acceleration is preserved.

## Purpose

The goal is to build "pick up an object and move it to a specified location" from the
ground up. Starting from standing up the robot with an already-provided URDF, it's built up
one layer at a time — controllers, motion planning, gripper, vision recognition. The final
goal also includes learning how to apply AI on top of all of this.
Rather than running someone else's demo, the goal of this repository is to climb the stack
while directly confirming, at each layer, why it's needed and what breaks without it.

### Why this specific use case

"Pick-and-place," a common scenario in industrial settings, was chosen as the simulation
target, with the concrete goal of judging whether a PCB-like board is the right way round
and seating it in a jig accordingly.
On top of that, assuming a high-mix, low-volume production scenario where the board/jig
shapes can change, the goal is also to see whether AI can be used to handle a changing work
environment without editing the robot's parameters.

The robot started out as a UR5e. That's overkill for this task's scale (board ~2-3cm, jig
~15cm), so it's set up so a smaller model like the UR3e can be swapped in with a single
`ur_type` argument (`src/arm_description/urdf/ur5e.urdf.xacro`).

## Underlay / Overlay

The workspace has the following underlay/overlay structure.

| | Contents | Location |
|---|---|---|
| Underlay | Upstream source for the UR driver, MoveIt 2, ros2_control, etc. | **Outside** this repo, at `../ur_ws` |
| Overlay | 3 hand-written packages | `src/` in this repo |

The underlay is Universal Robots' ROS package source. My own code is built as an overlay on
top of it.
This repository is **a single overlay workspace** — it contains only hand-written code.

Anything that can be reused as-is from upstream (robot shape macros, the SRDF's collision
pair list, etc.) is pulled in with a `$(find ur_description)`-style include. Only what
actually needed a decision (the `<ros2_control>` tag, controller yaml, gripper shape,
application logic) was written from scratch.

Which repo and branch the underlay pulls from is declared in `environment/ur_ws.repos`.

## Structure

The directory name on the host doesn't matter. `jazzy.sh` derives paths from its own script
location, and inside the container it's always mounted at `~/robot-arm-study`.

```
robot-arm-study/                     ← this repo (overlay). ~/robot-arm-study inside the container
├── environment/                     container environment
│   ├── Containerfile                ROS 2 Jazzy + Gazebo + MoveIt + VirtualGL
│   ├── build.sh                     build the image (auto-fills in host account/UID)
│   ├── jazzy.sh                     start and enter the container
│   ├── ur_ws.repos                  underlay source list (for vcs import)
│   └── .containerignore
├── src/
│   ├── arm_description/             shape — URDF/xacro, ros2_control declaration, SRDF
│   ├── arm_bringup/                 execution — launch files, controller/MoveIt config
│   └── arm_control_app/             application — C++ MoveGroupInterface node
└── docs/                            debugging notes, conventions
```

## Getting started

### 1. Prepare the underlay source (UR Robot ROS 2 package source — clone the git repos, not the binary packages)

The underlay lives outside this repository, as a sibling directory. It's needed so that
`rosdep` can pre-install dependency packages when building the image.

There are 14 repos to clone, so rather than `git clone`-ing them one by one, they're all
pulled in at once with `vcs` (vcstool). Which repo/branch to fetch is written in
`environment/ur_ws.repos`.

```bash
sudo apt install python3-vcstool     # if vcs isn't installed

cd ..                                # this repo's parent directory
mkdir -p ur_ws/src && cd ur_ws
vcs import src < ../robot-arm-study/environment/ur_ws.repos
```

The repos to clone fall into three broad areas.

| Area | Repositories |
|---|---|
| UR robot | `ur_driver`, `..._ROS2_Description`, `..._Client_Library`, `ur_simulation_gz`, `ur_msgs` |
| Motion planning | `moveit2`, `moveit_msgs`, `srdfdom` |
| Control | `ros2_control`, `ros2_controllers`, `control_msgs`, `control_toolbox`, `realtime_tools`, `kinematics_interface` |

Source is used instead of binaries because the point is to learn by directly reading the
upstream code, and this repo reuses `ur_description`'s shape macros and `ur_moveit_config`'s
settings via include. The debugging notes under `docs/` were also root-caused by reading
this source.

> **Warning** — When building the image from the `Containerfile`, do not install the UR
> binary packages via apt.
> In particular, **`ur-description`, `ur-moveit-config`, `ur-msgs`, and `ur-robot-driver`
> must not be installed as binaries.** Building from source and then running the UR
> simulation launch causes conflicts if the binaries are also present.
> The same warning is left as a comment in the `Containerfile`.

### 2. Build the container image

```bash
cd robot-arm-study/environment
./build.sh
```

`build.sh` bakes the current host account name and UID/GID straight into the image. That
way, files created inside the container show up as owned by you on the host too, and you
can edit the mounted workspace directly with a host-side editor.

The container comes with `vglrun` pre-installed so the 3D graphics accelerator can be used.
To actually get 3D acceleration inside the container, you'll need to set the graphics-card
related podman run options to match your own environment.

### 3. Start the container

```bash
./jazzy.sh
```

Mount layout (paths are derived automatically from the script's location):

| Host | Container |
|---|---|
| `robot-arm-study/` | `~/robot-arm-study` |
| `../ur_ws/` | `~/ur_ws` |

> **If you're working from VS Code** — instead of `./jazzy.sh`, open this repo in VS Code
> and run `Reopen in Container`. `.devcontainer/devcontainer.json` is set up to produce the
> same mount layout as `jazzy.sh` (`~/robot-arm-study`, `~/ur_ws`). The in-container path
> needs to match either way, or the paths recorded in `install/` end up inconsistent.

> The Dev Containers extension calls docker by default. To use podman instead, add the
> following to your VS Code settings file (`~/.config/Code/User/settings.json`):
>
> ```json
> "dev.containers.dockerPath": "podman",
> "dev.containers.dockerComposePath": "podman compose"
> ```

### 4. Build (inside the container)

Underlay first, then overlay. The sourcing order is the dependency order.

```bash
cd ~/ur_ws && colcon build            # first time only, takes a while
source install/setup.bash

cd ~/robot-arm-study && colcon build --symlink-install
source install/setup.bash
```

Because it's installed with `--symlink-install`, edits to launch/yaml/xacro files take
effect immediately without a rebuild.

### 5. Run (3 terminals)

```bash
# 1) Gazebo + controllers
ros2 launch arm_bringup arm_study_bringup.launch.py

# 2) MoveIt + RViz
ros2 launch arm_bringup move_group.launch.py

# 3) Application node — moves to the "home" pose from code
ros2 run arm_control_app arm_control_app
```

## Progress

Organized in learning order.

| Step | What was done | Status |
|---|---|---|
| 1. Container environment | Podman + ROS 2 Jazzy image, GPU rendering (VirtualGL), building the underlay source | Done |
| 2. Package skeleton and install rules | Created `arm_description`/`arm_bringup`. Confirmed with the `xacro` command that `install(DIRECTORY ...)` is required for `$(find ...)` to resolve | Done |
| 3. Writing the URDF and spawning in Gazebo | Reused `ur_description`'s `ur_macro.xacro`, fixed to the `world` link. Spawning without a controller confirmed that **the joints go limp under gravity and the robot collapses** — a robot with no controller is just an object that happens to have joints | Done |
| 4. Declaring ros2_control | `<ros2_control>` tag + `gz_ros2_control` plugin. Joint interfaces reused by including `ur_joint_control.xacro` | Done |
| 5. Controller config and activation | `joint_state_broadcaster` + `joint_trajectory_controller` in `arm_controllers.yaml`. Activated via spawner, checked status with `ros2 control list_controllers`. Confirmed the arm actually moves by injecting a trajectory directly from the command line | Done |
| 6. SRDF and MoveIt integration | Wrote the SRDF (collision-pair list reused from `ur_moveit_config`), `move_group` launch. Controlled the Gazebo arm via RViz's Plan/Execute | Done |
| 7. C++ application node | `MoveGroupInterface`: `setNamedTarget("home")` → `setPoseTarget`. Controlled directly from code, without RViz | Done |
| 8. Gripper | Added a parallel gripper shape (2-axis prismatic) on `tool0` | **In progress** |
| 9. Pick-up cycle | Cycle through supply tray → jig → discharge tray, performing an actual pick/place | Planned |
| 10. Orientation judgement and branching | Judge whether the board's orientation is correct; seat it in the jig if so, flip and retry if not. Handled as a state machine, since the flow branches on the result | Planned |
| 11. Vision | Recognize the board's position and orientation with a camera. Start with ArUco markers, designed so it can be swapped for YOLO later | Planned |
| 12. Applying AI | Check whether the robot can adapt to changing board/jig shapes without editing robot parameters | Planned |

The git repository was created partway through step 8. So the results of steps 1-7 are all
bundled into a single initial commit, and commits are split per step from that point on.

## License

[BSD 3-Clause](LICENSE). `ur_description`, MoveIt 2, and the other packages this repo
includes via `find`/include share the same license. The underlay's (`ur_ws`) upstream
source is not included in this repository and follows its own respective license.

## Debugging notes

Problems hit along the way, along with the root-cause analysis, are collected under
[`docs/`](docs/) (Korean only for now).

- [**MoveIt moves, but Gazebo doesn't**](docs/moveit-gazebo-controller-mismatch.md)
  — A problem hit while running the official UR examples from the underlay (`ur_ws`). The
  controller name MoveIt was sending commands to (`scaled_joint_trajectory_controller`,
  meant for the real robot) didn't match the name actually running in the simulator
  (`joint_trajectory_controller`), so the commands never arrived.

## Commit convention

Follows [Conventional Commits](docs/commit-convention.md) (Korean only for now).
