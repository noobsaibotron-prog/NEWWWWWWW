#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <iostream>

/**
 * Test runner with proper exit codes and optional category filtering / verbosity.
 */
class TestRunner
{
public:
    struct Options
    {
        juce::String category;
        bool verbose = false;
        bool runAll = false;
    };

    static Options parseArgs(int argc, char** argv)
    {
        Options opts;
        for (int i = 1; i < argc; ++i)
        {
            juce::String arg(argv[i]);
            if (arg.startsWith("--category="))
                opts.category = arg.fromFirstOccurrenceOf("=", false, false);
            else if (arg == "--verbose" || arg == "-v")
                opts.verbose = true;
            else if (arg == "--all")
                opts.runAll = true;
        }
        return opts;
    }

    static int run(const Options& opts)
    {
        juce::UnitTestRunner runner;

        std::cout << "========================================" << std::endl;
        std::cout << "     AI Equalizer Pro - Test Suite      " << std::endl;
        std::cout << "========================================" << std::endl;

        if (opts.category.isNotEmpty())
        {
            std::cout << "Running category: " << opts.category << std::endl;
            runner.runTestsInCategory(opts.category);
        }
        else if (opts.runAll)
        {
            std::cout << "Running all registered tests..." << std::endl;
            runner.runAllTests();
        }
        else
        {
            const juce::StringArray projectCategories { "DSP", "Regression", "Integration" };
            std::cout << "Running project test categories: "
                      << projectCategories.joinIntoString(", ") << std::endl;

            for (const auto& category : projectCategories)
                runner.runTestsInCategory(category);
        }

        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "              RESULTS                   " << std::endl;
        std::cout << "========================================" << std::endl;

        int totalTests = 0;
        int totalPasses = 0;
        int totalFailures = 0;

        for (int i = 0; i < runner.getNumResults(); ++i)
        {
            if (const auto* result = runner.getResult(i))
            {
                totalPasses += result->passes;
                totalFailures += result->failures;
                totalTests += result->passes + result->failures;

                const juce::String status = (result->failures == 0) ? "[PASS]" : "[FAIL]";
                std::cout << status << " " << result->unitTestName << std::endl;

                if (opts.verbose && result->messages.size() > 0)
                {
                    for (const auto& message : result->messages)
                        std::cout << "       " << message << std::endl;
                }
            }
        }

        std::cout << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Total assertions: " << totalTests << std::endl;
        std::cout << "Passed:           " << totalPasses << std::endl;
        std::cout << "Failed:           " << totalFailures << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        if (totalFailures == 0)
        {
            std::cout << "✓ ALL TESTS PASSED" << std::endl;
            return 0;
        }

        std::cout << "✗ " << totalFailures << " TEST(S) FAILED" << std::endl;
        return 1;
    }
};

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    auto options = TestRunner::parseArgs(argc, argv);
    const int result = TestRunner::run(options);
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
    return result;
}

