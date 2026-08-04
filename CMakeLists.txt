cmake_minimum_required(VERSION 3.10)
project(EFI-Mac-Emulator)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(
    src
    src/cpu
    src/memory
    src/hardware
    src/boot
    src/utils
    src/platform
    ${EFI_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.c
    
    # CPU Translation Layer
    src/cpu/translation_impl.c
    
    # Memory Management
    src/memory/manager_impl.c
    
    # Hardware Abstraction
    src/hardware/abstraction_impl.c
    
    # Bootloader
    src/boot/bootloader_impl.c
    
    # Utilities
    src/utils/debug_impl.c
    
    # UEFI Interface
    src/platform/uefi_interface_impl.c
)

# Create the EFI application
add_executable(${PROJECT_NAME} ${SOURCES})

# Set target properties for EFI
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "EFI-Mac-Emulator"
    PREFIX ""
)

# Link with required libraries (these would be provided by EDK II or similar)
# target_link_libraries(${PROJECT_NAME} 
#     # Add UEFI libraries here
# )

# Set compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # Define any needed compile time constants here
)

# Set compiler flags for EFI development
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fno-stack-protector
        -ffreestanding
        -fno-builtin
        -Wall
        -Wextra
    )
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fno-stack-protector
        -ffreestanding
        -fno-builtin
        -Wall
        -Wextra
    )
endif()

# Install target (optional)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Add a custom target for building UEFI application
add_custom_target(build_efi ALL
    DEPENDS ${PROJECT_NAME}
    COMMENT "Building EFI-Mac-Emulator"
)

message(STATUS "EFI-Mac-Emulator CMakeLists.txt created successfully")