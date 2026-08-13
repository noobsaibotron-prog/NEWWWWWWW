cmake_minimum_required(VERSION 3.22)
project(AIEQMotoreV3LabTests NONE)

enable_testing()

# This is deliberately a separate CTest project.  The shipping root CMake must
# not reference or package the pre-GO laboratory; CI opts into this lab project
# explicitly and supplies its frozen interpreter.
set(AIEQ_ML_V3_PYTHON "" CACHE FILEPATH
    "Frozen CPython interpreter used for the complete Motore v3 test gate")
if(NOT AIEQ_ML_V3_PYTHON)
    message(FATAL_ERROR "AIEQ_ML_V3_PYTHON is required")
endif()
if(NOT EXISTS "${AIEQ_ML_V3_PYTHON}")
    message(FATAL_ERROR
        "AIEQ_ML_V3_PYTHON does not exist: ${AIEQ_ML_V3_PYTHON}")
endif()

get_filename_component(AIEQ_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

add_test(NAME aieq_ml_v3_regression
         COMMAND "${AIEQ_ML_V3_PYTHON}" -B -m unittest discover
                 -s ml_v3/tests -t . -q
         WORKING_DIRECTORY "${AIEQ_REPO_ROOT}")
set_tests_properties(aieq_ml_v3_regression PROPERTIES
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1"
    LABELS "LabV3;Regression"
    TIMEOUT 900)
