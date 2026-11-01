/* The model editor's PS1 view: the port's software GPU (pc/gpu.c) as a library for the editor's own Python, with a
 * call that sends a whole list of GP0 words at once (a call per word from Python is slow). `make editor` builds it. */
#include "gpu.h"

void gpu_gp0_words(const unsigned int *words, int count)
{
    for (int i = 0; i < count; i++)
        gpu_gp0(words[i]);
}
