cmake_minimum_required(VERSION 3.28)

project(MotorControlUnit VERSION 1.0 LANGUAGES C CXX)

# Add subdirectory for AxisController before creating the executable
add_subdirectory(MountControlUnit/AxisController)

# Create executable after building the AxisController
add_executable(MotorControlUnit MountControlUnit/MountController/Test.cpp)

# Link AxisController library to MotorControlUnit executable
target_link_libraries(MotorControlUnit PRIVATE axis_controller)
