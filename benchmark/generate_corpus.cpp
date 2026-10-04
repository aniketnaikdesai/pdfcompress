#include "test_corpus_generator.h"
#include <iostream>

int main() {
    pdfcompress::test::TestCorpusGenerator generator;
    generator.generateAll();
    std::cout << "Corpus generated at: " << generator.corpusDir() << std::endl;
    return 0;
}
