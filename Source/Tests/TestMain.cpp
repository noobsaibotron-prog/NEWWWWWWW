#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <cstdlib>
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
        juce::String nameContains;
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
            else if (arg.startsWith("--name="))
                opts.nameContains = arg.fromFirstOccurrenceOf("=", false, false);
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
            "AI-RT",
            "AI-Sweep",
            "ClickTests",
            "Core",
            // Renamed from "DSP" for FA-002. juce_dsp registers its own unit tests
            // under UnitTestCategories::dsp, which is the string "DSP" — the same
            // category ours used. While both shared it, a category could not be the
            // authority for selection, and the gate fell back to matching the test
            // NAME against a prefix list. That made the name a scheduling mechanism:
            // four blocking tests had no gate that ran them at all, and a test was
            // once silently skipped for want of a prefix while the suite read green.
            // "DSP" must NOT be added back here: doing so pulls the nine juce_dsp
            // tests into every Ember gate.
            "AIEQ-DSP",
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

    // isAieqProjectTestName() lived here and gated the default run on the test's
    // NAME. It is gone: with the category collision removed above, membership of
    // a test executable plus a declared blocking category is a complete and
    // honest authority, and a test's name is free to describe the test.
    // MetaTestSelectionIntegrityTest keeps this from regressing.


    static int run(const Options& opts)
    {
        std::cout << "========================================" << std::endl;
        std::cout << "     AI Equalizer Pro - Test Suite      " << std::endl;
        std::cout << "========================================" << std::endl;

        Summary summary;

        if (opts.nameContains.isNotEmpty())
        {
            juce::Array<juce::UnitTest*> tests;
            for (auto* t : juce::UnitTest::getAllTests())
                if (t != nullptr && t->getName().containsIgnoreCase(opts.nameContains))
                    tests.add(t);

            if (tests.isEmpty())
            {
                std::cout << "No tests matched --name=" << opts.nameContains << std::endl;
                return 1;
            }

            std::cout << "Running tests matching: " << opts.nameContains << std::endl;
            juce::UnitTestRunner runner;
            runner.runTests(tests);
            accumulateResults(runner, summary, opts.verbose);
        }
        else if (opts.category.isNotEmpty())
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

                if (isDefaultBlockingCategory(t->getCategory()))
                {
                    tests.add(t);
                    categories.addIfNotAlreadyThere(t->getCategory());
                }
            }

            categories.sort(true);

            // FA-002 tripwire. Selection is now purely categorical, so this holds
            // by construction — which is exactly why it is worth asserting: if a
            // name filter or any other extra predicate is ever reintroduced here,
            // this fails immediately and names what it dropped, instead of the
            // suite going quietly green over a test nobody runs.
            {
                juce::StringArray dropped;
                for (auto* t : juce::UnitTest::getAllTests())
                    if (t != nullptr
                        && isDefaultBlockingCategory(t->getCategory())
                        && ! tests.contains(t))
                        dropped.add(t->getName() + " [" + t->getCategory() + "]");

                if (! dropped.isEmpty())
                {
                    std::cout << "FAILED: blocking tests registered but not selected:"
                              << std::endl;
                    for (const auto& d : dropped)
                        std::cout << "  - " << d << std::endl;
                    return 1;
                }
            }

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
    // Never let unit tests handshake against a live Observer rendezvous.
    setenv("EMBER_PROPOSAL_DISABLE_AUTOCONNECT", "1", 1);
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    auto options = TestRunner::parseArgs(argc, argv);
    const int result = TestRunner::run(options);
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
    return result;
}
