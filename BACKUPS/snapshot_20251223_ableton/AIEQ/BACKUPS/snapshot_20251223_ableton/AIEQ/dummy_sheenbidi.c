// dummy_sheenbidi.c
// Stub minimalista per soddisfare il linker su Windows
// ATTENZIONE: qui disattiviamo il vero algoritmo bidi di SheenBidi.
// Va benissimo per UI LTR "normali" (inglese/italiano), ma non per script complessi.

typedef void* SBAlgorithmRef;
typedef void* SBParagraphRef;
typedef void* SBLineRef;
typedef unsigned int SBCodepoint;

// Categoria generale, script e tipo bidi li trattiamo come semplici interi.
int SBCodepointGetGeneralCategory(SBCodepoint codepoint) { (void)codepoint; return 0; }
int SBCodepointGetScript(SBCodepoint codepoint)          { (void)codepoint; return 0; }
int SBCodepointGetBidiType(SBCodepoint codepoint)        { (void)codepoint; return 0; }

// Algoritmo / paragrafo / linee: usiamo tutti void* e int,
// tanto il chiamante non sa che stiamo restituendo dummy.
SBAlgorithmRef SBAlgorithmCreate(void* sequence, int a, int b, int c)
{
    (void)sequence; (void)a; (void)b; (void)c;
    return 0;
}

void SBAlgorithmRelease(SBAlgorithmRef algorithm)
{
    (void)algorithm;
}

SBParagraphRef SBAlgorithmCreateParagraph(SBAlgorithmRef algorithm, int a, int b, int c)
{
    (void)algorithm; (void)a; (void)b; (void)c;
    return 0;
}

SBParagraphRef SBParagraphRetain(SBParagraphRef paragraph)
{
    // Retain fittizio: semplicemente ritorniamo il puntatore passato.
    return paragraph;
}

void SBParagraphRelease(SBParagraphRef paragraph)
{
    (void)paragraph;
}

int SBParagraphGetLength(SBParagraphRef paragraph)
{
    (void)paragraph;
    return 0;
}

const unsigned char* SBParagraphGetLevelsPtr(SBParagraphRef paragraph)
{
    (void)paragraph;
    // Livelli fittizi: un singolo livello neutro.
    static const unsigned char dummyLevel = 0;
    return &dummyLevel;
}

SBLineRef SBParagraphCreateLine(SBParagraphRef paragraph, int start, int length)
{
    (void)paragraph; (void)start; (void)length;
    return 0;
}

int SBLineGetLength(SBLineRef line)
{
    (void)line;
    return 0;
}

int SBLineGetRunCount(SBLineRef line)
{
    (void)line;
    return 0;
}

void* SBLineGetRunsPtr(SBLineRef line)
{
    (void)line;
    return 0;
}

void SBLineRelease(SBLineRef line)
{
    (void)line;
}
