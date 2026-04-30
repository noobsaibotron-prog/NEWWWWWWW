#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <iostream>

class HarnessSelfTest : public juce::UnitTest
{
public:
    HarnessSelfTest() : juce::UnitTest("Harness Self-Test", "Meta") {}

    void runTest() override
    {
        beginTest("Failure propagation sanity check");

        if (juce::SystemStats::getEnvironmentVariable("AIEQ_HARNESS_SELFTEST", "0") == "1")
            expect(false, "Synthetic failure to verify harness reporting");
    }
};

static HarnessSelfTest harnessSelfTest;

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

    struct Summary
    {
        int totalAssertions = 0;
        int totalPasses = 0;
        int totalFailures = 0;
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
        std::cout << "========================================" << std::endl;
        std::cout << "     AI Equalizer Pro - Test Suite      " << std::endl;
        std::cout << "========================================" << std::endl;

        Summary summary;

        if (opts.category.isNotEmpty())
        {
            std::cout << "Running category: " << opts.category << std::endl;
            juce::UnitTestRunner runner;
            runner.runTestsInCategory(opts.category);
            accumulateResults(runner, summary, opts.verbose);
        }
        else if (opts.runAll)
        {
            std::cout << "Running all registered tests..." << std::endl;
            juce::UnitTestRunner runner;
            runner.runAllTests();
            accumulateResults(runner, summary, opts.verbose);
        }
        else
        {
            const juce::StringArray projectCategories { "DSP", "Regression", "Integration" };
            std::cout << "Running project test categories: "
                      << projectCategories.joinIntoString(", ") << std::endl;

            for (const auto& category : projectCategories)
            {
                juce::UnitTestRunner runner;
                runner.runTestsInCategory(category);
                accumulateResults(runner, summary, opts.verbose);
            }
        }

        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "              RESULTS                   " << std::endl;
        std::cout << "========================================" << std::endl;

        std::cout << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Total assertions: " << summary.totalAssertions << std::endl;
        std::cout << "Passed:           " << summary.totalPasses << std::endl;
        std::cout << "Failed:           " << summary.totalFailures << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        if (summary.totalFailures == 0)
        {
            std::cout << "ALL TESTS PASSED" << std::endl;
            return 0;
        }

        std::cout << "FAILED: " << summary.totalFailures << " TEST(S) FAILED" << std::endl;
        return 1;
    }

private:
    static void accumulateResults(const juce::UnitTestRunner& runner,
                                  Summary& summary,
                                  bool verbose)
    {
        for (int i = 0; i < runner.getNumResults(); ++i)
        {
            if (const auto* result = runner.getResult(i))
            {
                summary.totalPasses += result->passes;
                summary.totalFailures += result->failures;
                summary.totalAssertions += result->passes + result->failures;

                const juce::String status = (result->failures == 0) ? "[PASS]" : "[FAIL]";
                std::cout << status << " " << result->unitTestName << std::endl;

                if (verbose && result->messages.size() > 0)
                {
                    for (const auto& message : result->messages)
                        std::cout << "       " << message << std::endl;
                }
            }
        }
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
