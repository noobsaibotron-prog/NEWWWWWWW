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
        expect(true, "Harness reports at least one assertion on healthy runs");

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
        int totalTests = 0;
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

    static bool isDefaultBlockingCategory(const juce::String& category)
    {
        static constexpr const char* categories[] = {
            "AI",
            "AI-Calibration",
            "AI-Contract",
            "AI-Corpus",
            "AI-Diag",
            "AI-Front",
            "AI-Integration",
            "AI-Knobs",
            "AI-Sweep",
            "ClickTests",
            "Core",
            "DSP",
            "Integration",
            "Perceptual",
            "Performance",
            "RealData",
            "Regression",
            "ThreadSafety",
        };

        for (const auto* projectCategory : categories)
            if (category == projectCategory)
                return true;

        return false;
    }

    static bool isAieqProjectTestName(const juce::String& name)
    {
        static constexpr const char* prefixes[] = {
            "AI",
            "AIEqualizer",
            "Anti-Pop",
            "Band Drag",
            "BiquadCoeffs",
            "BlockSize",
            "Bypass",
            "CaptureService",
            "D1 ",
            "DynEQ",
            "Dynamic",
            "EQ Graph",
            "Frame coherence",
            "Freq Drag",
            "Fuzz BlockSize",
            "Host Session",
            "Integration",
            "LinearPhase",
            "ML ",
            "Motore",
            "MS ",
            "Oversampling",
            "ParametricEQProcessor",
            "Perceptual",
            "Phase Mode",
            "RB",
            "Real ",
            "Real-data",
            "SmoothedValue",
            "Solo Mode",
            "Spectrum",
        };

        for (const auto* prefix : prefixes)
            if (name.startsWith(prefix))
                return true;

        return false;
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
            // Exhaustive: EVERYTHING including the KnownDebt quarantine. May be
            // red while documented debt is still real — that is expected.
            std::cout << "Running ALL registered tests (incl. KnownDebt)..." << std::endl;
            juce::UnitTestRunner runner;
            runner.runAllTests();
            accumulateResults(runner, summary, opts.verbose);
        }
        else
        {
            // N1 FIX (honest gates): the no-arg run executes EVERY registered
            // project category. Rationale:
            //  - The old default ran only {DSP, Regression, Integration},
            //    silently skipping every AI-* category + Perceptual/Performance/
            //    ClickTests/Meta. A later broad default also picked up JUCE's
            //    built-in self-tests, which can make a binary with no project
            //    tests look green. The default-run count must mean "all BLOCKING
            //    project tests in this binary", never a hidden or framework-only
            //    subset.
            //  - "KnownDebt" is the documented, non-blocking quarantine for
            //    pre-existing failures (test-harness layout hazards, synthetic-
            //    fixture realism, real ML/DSP debt). It is excluded here so the
            //    default gate is green-with-known-debt, and run explicitly via
            //    `--category=KnownDebt`. `--all` still runs EVERYTHING including
            //    KnownDebt (exhaustive, may be red while debt is real).
            //  - "Meta" is a harness self-test. It is useful for explicit
            //    `--category=Meta` checks, but it must not make a binary with no
            //    product tests look green.
            juce::Array<juce::UnitTest*> tests;
            juce::StringArray categories;

            for (auto* t : juce::UnitTest::getAllTests())
            {
                if (t == nullptr)
                    continue;

                if (isDefaultBlockingCategory(t->getCategory()) && isAieqProjectTestName(t->getName()))
                {
                    tests.add(t);
                    categories.addIfNotAlreadyThere(t->getCategory());
                }
            }

            categories.sort(true);

            std::cout << "Running blocking AIEQ project tests in categories: "
                      << categories.joinIntoString(", ") << std::endl;

            juce::UnitTestRunner runner;
            runner.runTests(tests);
            accumulateResults(runner, summary, opts.verbose);
        }

        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "              RESULTS                   " << std::endl;
        std::cout << "========================================" << std::endl;

        std::cout << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Total tests:      " << summary.totalTests << std::endl;
        std::cout << "Total assertions: " << summary.totalAssertions << std::endl;
        std::cout << "Passed:           " << summary.totalPasses << std::endl;
        std::cout << "Failed:           " << summary.totalFailures << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        if (summary.totalTests == 0)
        {
            std::cout << "FAILED: no tests were executed" << std::endl;
            return 1;
        }

        if (summary.totalAssertions == 0)
        {
            std::cout << "FAILED: tests executed without assertions" << std::endl;
            return 1;
        }

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
                ++summary.totalTests;
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
