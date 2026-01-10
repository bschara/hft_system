include(CTest)
enable_testing()
# -----------------------------
# GoogleTest
# -----------------------------
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG main  # latest commit from main branch
)

# Prevent overriding parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# -----------------------------
# Add test subdirectories
# -----------------------------
add_subdirectory(market_data)
# add_subdirectory(utils)
# add_subdirectory(order_gateway)
# add_subdirectory(strategies)
# add_subdirectory(pcm_model)
# add_subdirectory(oms)