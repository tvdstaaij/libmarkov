#ifndef LIBMARKOV_H
#define LIBMARKOV_H

#include <climits>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

const std::uint32_t uiMarkovMaxStates = UINT32_MAX - 1;
const std::size_t szMarkovMaxWordLength = 1023;
const std::vector<std::string> vsWhitespaceCharacters = {" ", "\t", "\x0A", "\r"};

class MarkovNode; // Forward declaration to allow recursive reference to self
typedef std::vector<const std::string*> markov_candidatelist_t;
// These unordered maps could as well be ordered, but
// performance is measured to be better with unordered variants.
typedef std::unordered_map<const std::string*,MarkovNode*> markov_treemap_t;
typedef std::unordered_set<std::string> markov_wordlist_t;

enum MarkovOutputMode
{
    MARKOV_MODE_RANDOM,
    MARKOV_MODE_PROBABLE
};

class MarkovException : public std::runtime_error
{
public:
    MarkovException();
    MarkovException(std::string sDescription);
};

class MarkovMissingWordException : public MarkovException
{
};

class MarkovIOException : public MarkovException
{
};

class MarkovModelEmptyException : public MarkovException
{
};

class MarkovConstraintException : public MarkovException
{
};

/**
 * @brief Container for a state consisting of a sequence of words. For internal use.
 */
class MarkovWordSequence
{
public:
    MarkovWordSequence(std::size_t szCapacity);
    MarkovWordSequence(const std::vector<std::string>& vsWords);
    std::uint32_t getWordCount() const;
    std::string getLastWord() const;
    const std::vector<std::string>& getWords() const;
    friend bool operator==(const MarkovWordSequence& mwA,
                           const MarkovWordSequence& mwB);
    friend bool operator!=(const MarkovWordSequence& mwA,
                           const MarkovWordSequence& mwB);
    void clearWords();
    void shiftWordIn(std::string sWord);
    void shiftToSize(std::uint32_t uiSize);
    void addWord(std::string sWord);
    void addOrShiftWord(std::string sWord, std::uint32_t uiMaxWordCount);
private:
    std::vector<std::string> vsWords;
};


/**
 * @brief Pure virtual base class for word sources that can be fed to a Markov model.
 */
class MarkovWordSource
{
public:
    virtual ~MarkovWordSource();
    virtual std::string getNextWord() = 0;
};

/**
 * @brief Word source based on a C++ standard library input stream.
 */
class StreamingMarkovWordSource : public MarkovWordSource
{
public:
    StreamingMarkovWordSource(std::istream& isWordStream,
                              const std::vector<std::string>& vsWordSeparators
                              = vsWhitespaceCharacters);
    std::string getNextWord();
private:
    std::istream& isWordStream;
    const std::vector<std::string> vsWordSeparators;
    std::string sBuffer;
    int iErrno;
};

/**
 * @brief MarkovWordSource based on a pre-constructed vector of words.
 */
class StaticMarkovWordSource : public MarkovWordSource
{
public:
    StaticMarkovWordSource(const std::vector<std::string>& vsWords);
    std::string getNextWord();
private:
    const std::vector<std::string>& vsWords;
    std::vector<std::string>::const_iterator ivsWordIterator;
};

/**
 * @brief Wrapper for a lookup table of words. For internal use.
 */
class MarkovWordCollection
{
public:
    std::uint32_t getWordCount() const;
    const std::string* resolveWord(const std::string& sWord);
private:
    markov_wordlist_t ssWords;
};

/**
 * @brief Node in the Markov model tree. For internal use.
 */
class MarkovNode
{
public:
    MarkovNode();
    ~MarkovNode();
    const MarkovNode* findChild(const std::string* psNextWord) const;
    std::pair<const std::string*,const MarkovNode*>
        getRandomChild(std::mt19937& mtRandomSource) const;
    const std::string*
        getRandomCandidate(std::mt19937& mtRandomSource) const;
    std::uint32_t getCandidateCount() const;
    MarkovNode& resolveChild(const std::string* psNextWord);
    void addCandidate(const std::string* psCandidate);
private:
    markov_treemap_t* pmmnChildren;
    markov_candidatelist_t* pvsCandidates;
};

/**
 * @brief Main class representing a Markov model that can be fed and used to output words.
 */
class MarkovChain
{
public:
    MarkovChain(std::uint8_t uiDepth = 1);
    std::uint8_t getDepth() const;
    std::uint32_t getStateCount() const;
    std::uint32_t getUniqueWordCount() const;
    void setOutputMode(MarkovOutputMode mmOutputMode);
    void reset();
    void feedModel(MarkovWordSource& mcSource,
                   std::uint32_t* puiProcessedWordCount = nullptr);
    std::string generateWord(bool* pbResetOccurred = nullptr);
private:
    const std::uint8_t uiDepth;
    MarkovOutputMode mmOutputMode;
    MarkovNode mnTreeRoot;
    MarkovWordCollection mcWordCollection;
    MarkovWordSequence msCurrentState;
    std::mt19937 mtRandomSource;
};

#endif
