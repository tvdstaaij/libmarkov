#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include "libmarkov.h"

using namespace std;

uint32_t fitValueWithinRange(uint32_t uiValue, uint32_t uiMin, uint32_t uiMax);

template<class T>
const T* selectRandomElement(const vector<T>& vContainer, mt19937& mtRandomSource)
{
    size_t uiContainerSize = vContainer.size();
    if (uiContainerSize <= 0)
    {
        return nullptr;
    }
    uint32_t uiRandomNumber = mtRandomSource();
    return &vContainer[uiContainerSize <= 1 ? 0 :
                       fitValueWithinRange(uiRandomNumber, 0, uiContainerSize - 1)];
}

template<class Key,class T>
pair<const Key*,const T*> selectRandomElement(const unordered_map<Key,T>& mContainer, mt19937& mtRandomSource)
{
    size_t iContainerSize = mContainer.size();
    if (iContainerSize <= 0)
    {
        return make_pair(nullptr, nullptr);
    }
    uint32_t uiRandomNumber = mtRandomSource();
    class unordered_map<Key,T>::const_iterator imIterator = mContainer.begin();
    uint32_t uiOffset = 0;
    if (iContainerSize > 1)
    {
        uiOffset = fitValueWithinRange(uiRandomNumber, 0, iContainerSize - 1);
    }
    else
    {
        uiOffset = 0;
    }
    if (uiOffset)
    {
        advance(imIterator, uiOffset);
    }
    return make_pair(&imIterator->first, &imIterator->second);
}

const std::vector<string>& MarkovWordSequence::getWords() const
{
    return vsWords;
}

string MarkovWordSequence::getLastWord() const
{
    if (getWordCount() < 1)
    {
        throw MarkovMissingWordException();
    }
    return vsWords.back();
}

uint32_t MarkovWordSequence::getWordCount() const
{
    return vsWords.size();
}

MarkovWordSequence::MarkovWordSequence(const vector<string>& vsWords)
    : vsWords(vsWords)
{
}

MarkovWordSequence::MarkovWordSequence(size_t szCapacity)
{
    vsWords.reserve(szCapacity);
}

void MarkovWordSequence::shiftWordIn(string sWord)
{
    if (getWordCount() == 1)
    {
        vsWords[0] = sWord;
        return;
    }
    shiftToSize(vsWords.size() - 1);
    addWord(sWord);
}

void MarkovWordSequence::shiftToSize(uint32_t uiSize)
{
    while (vsWords.size() > uiSize)
    {
        vsWords.erase(vsWords.begin());
    }
}

void MarkovWordSequence::addWord(string sWord)
{
    vsWords.push_back(sWord);
}

void MarkovWordSequence::addOrShiftWord(string sWord, uint32_t uiMaxWordCount)
{
    if (!getWordCount() || getWordCount() < uiMaxWordCount)
    {
        addWord(sWord);
    }
    else
    {
        shiftWordIn(sWord);
    }
    shiftToSize(uiMaxWordCount);
}

void MarkovWordSequence::clearWords()
{
    vsWords.clear();
}

bool operator==(const MarkovWordSequence& mwA, const MarkovWordSequence& mwB)
{
    return mwA.vsWords == mwB.vsWords;
}

bool operator!=(const MarkovWordSequence& mwA, const MarkovWordSequence& mwB)
{
    return !(mwA == mwB);
}

MarkovWordSource::~MarkovWordSource()
{
}

/**
 * Construct a word stream from a C++ istream.
 * @param[in] isWordStream the istream to wrap
 * @param[in] vsWordSeparators list of character sequences that should be
 *            interpreted as end-of-word (e.g. space and newline)
 */
StreamingMarkovWordSource::StreamingMarkovWordSource(istream& isWordStream,
                                                     const vector<string>& vsWordSeparators)
    : isWordStream(isWordStream), vsWordSeparators(vsWordSeparators), iErrno(0)
{
    sBuffer.reserve(szMarkovMaxWordLength);
}

string StreamingMarkovWordSource::getNextWord()
{
    vector<string::const_iterator> visSeparatorMatches;
    for (size_t i = 0; i < vsWordSeparators.size(); i++)
    {
        visSeparatorMatches.push_back(vsWordSeparators[i].begin());
    }
    if (isWordStream.eof())
    {
        return "";
    }
    sBuffer.clear();
    char cBuffer;
    while (true)
    {
        isWordStream.get(cBuffer);
        if (isWordStream.eof() && !isWordStream.bad())
        {
            return sBuffer;
        }
        else if (!isWordStream.good())
        {
            throw MarkovIOException();
        }
        sBuffer += cBuffer;
        if (vsWordSeparators.empty())
        {
            return sBuffer;
        }
        if (sBuffer.length() > szMarkovMaxWordLength)
        {
            throw MarkovConstraintException();
        }
        for (size_t i = 0; i < visSeparatorMatches.size(); i++)
        {
            if (visSeparatorMatches[i] == vsWordSeparators[i].end())
            {
                continue;
            }
            if (cBuffer == *visSeparatorMatches[i])
            {
                if (++visSeparatorMatches[i] == vsWordSeparators[i].end())
                {
                    size_t szWordLength = sBuffer.length() - vsWordSeparators[i].length();
                    if (szWordLength > 0)
                    {
                        return sBuffer.substr(0, szWordLength);
                    }
                    sBuffer.clear();
                    for (size_t i = 0; i < visSeparatorMatches.size(); i++)
                    {
                        visSeparatorMatches[i] = vsWordSeparators[i].begin();
                    }
                    break;
                }
            }
            else
            {
                visSeparatorMatches[i] = vsWordSeparators[i].begin();
            }
        }
    }
    return "";
}

/**
 * Construct a word stream from a C++ vector containing the words.
 * @param[in] isWordStream the vector to wrap
 */
StaticMarkovWordSource::StaticMarkovWordSource(const vector<string>& vsWords)
    : vsWords(vsWords), ivsWordIterator(vsWords.begin())
{
}

string StaticMarkovWordSource::getNextWord()
{
    if (ivsWordIterator == vsWords.end())
    {
        return "";
    }
    ++ivsWordIterator;
    string sNextWord = *(ivsWordIterator - 1);
    if (sNextWord.length() > szMarkovMaxWordLength)
    {
        throw MarkovConstraintException();
    }
    return sNextWord;
}

MarkovNode::MarkovNode()
    : pmmnChildren(nullptr), pvsCandidates(nullptr)
{
}

MarkovNode::~MarkovNode()
{
    // Recursively delete child nodes
    if (pmmnChildren != nullptr)
    {
        for (pair<const string* const,MarkovNode*> prChild: *pmmnChildren)
        {
            delete prChild.second;
        }
    }
    delete pmmnChildren;
    delete pvsCandidates;
}

const MarkovNode* MarkovNode::findChild(const string* psNextWord) const
{
    if (pmmnChildren == nullptr)
    {
        return nullptr;
    }
    markov_treemap_t::const_iterator immnNode = (*pmmnChildren).find(psNextWord);
    if (immnNode == (*pmmnChildren).end())
    {
        return nullptr;
    }
    return immnNode->second;
}

const string* MarkovNode::getRandomCandidate(mt19937& mtRandomSource) const
{
    if (pvsCandidates == nullptr || pvsCandidates->empty())
    {
        return nullptr;
    }
    const string* const* ppsCandidate = selectRandomElement(*pvsCandidates, mtRandomSource);
    if (ppsCandidate == nullptr || *ppsCandidate == nullptr)
    {
        // Should never happen
        throw runtime_error("failed to select random canditate out of non-empty candidate list");
    }
    return *ppsCandidate;
}

uint32_t MarkovNode::getCandidateCount() const
{
    if (pvsCandidates == nullptr)
    {
        return 0;
    }
    return (*pvsCandidates).size();
}

// Return reference to the requested node, either existing or created on the fly
MarkovNode& MarkovNode::resolveChild(const string* psNextWord)
{
    MarkovNode* pmnNewNode = new MarkovNode();
    if (pmmnChildren == nullptr)
    {
        pmmnChildren = new markov_treemap_t;
    }
    pair<markov_treemap_t::iterator,bool> prInsertionResult = (*pmmnChildren).insert(make_pair(psNextWord, pmnNewNode));
    if (!prInsertionResult.second)
    {
        delete pmnNewNode;
    }
    return *(prInsertionResult.first->second);
}

void MarkovNode::addCandidate(const string* psCandidate)
{
    if (pvsCandidates == nullptr)
    {
        pvsCandidates = new markov_candidatelist_t;
    }
    (*pvsCandidates).push_back(psCandidate);
}

pair<const string*,const MarkovNode*> MarkovNode::getRandomChild(mt19937& mtRandomSource) const
{
    if (pmmnChildren == nullptr)
    {
        return make_pair(nullptr, nullptr);
    }
    pair<const string* const*,const MarkovNode* const*> prChild = selectRandomElement(*pmmnChildren, mtRandomSource);
    const string* const* ppsWord = prChild.first;
    const MarkovNode* const* ppmnNode = prChild.second;
    const string* psWord = (ppsWord == nullptr) ? nullptr : *ppsWord;
    const MarkovNode* pmnNode = (ppmnNode == nullptr) ? nullptr : *ppmnNode;
    return make_pair(psWord, pmnNode);
}

const string* MarkovWordCollection::resolveWord(const string& sWord)
{
    return &(*ssWords.insert(sWord).first);
}

uint32_t MarkovWordCollection::getWordCount() const
{
    return ssWords.size();
}

/**
 * Construct an empty Markov model.
 * @param[in] uiDepth The model's desired depth, i.e. the Markov chain order. Immutable.
 */
MarkovChain::MarkovChain(uint8_t uiDepth)
    : uiDepth(uiDepth), mmOutputMode(MARKOV_MODE_RANDOM),
      msCurrentState(uiDepth), mtRandomSource()
{
    random_device rdTrueRandomSource;
    mtRandomSource.seed(rdTrueRandomSource());
}

/**
 * Get the model's depth as set at construction time.
 * @return markov chain order
 */
uint8_t MarkovChain::getDepth() const
{
    return uiDepth;
}

/**
 * Recursively count the number of nodes in the Markov tree.
 * @return number of states
 * @bug not implemented yet; always returns zero
 */
uint32_t MarkovChain::getStateCount() const
{
    return 0;
}

/**
 * Get the number of unique analyzed words in the model's lookup table.
 * @return word associated with the current state
 */
uint32_t MarkovChain::getUniqueWordCount() const
{
    return mcWordCollection.getWordCount();
}

/**
 * Set the mode for word generation. Currently, only random mode is actually supported.
 * @param[in] mmOutputMode word generation mode
 * @todo implement probable mode
 */
void MarkovChain::setOutputMode(MarkovOutputMode mmOutputMode)
{
    this->mmOutputMode = mmOutputMode;
}

/**
 * Pick a random entry point for the word chain.
 */
void MarkovChain::reset()
{
    msCurrentState.clearWords();
    const MarkovNode* pmnRootNode = &mnTreeRoot;
    const MarkovNode* const* ppmnCurrentNode = &pmnRootNode;
    do {
        pair<const string*,const MarkovNode*> prNextChild = (*ppmnCurrentNode)->getRandomChild(mtRandomSource);
        const string* psCurrentWord = prNextChild.first;
        ppmnCurrentNode = &prNextChild.second;
        if (psCurrentWord != nullptr)
        {
            msCurrentState.addWord(*psCurrentWord);
        }
    } while (ppmnCurrentNode != nullptr && *ppmnCurrentNode != nullptr);
    if (msCurrentState.getWordCount() < 1)
    {
        throw MarkovModelEmptyException();
    }
}

/**
 * Analyze a stream of words and append to the current model.
 * @param[in] mcSource stream of words to be analyzed (read to end)
 * @param[out] puiProcessedWordCount constantly updated with the number of words read from the source
 */
void MarkovChain::feedModel(MarkovWordSource& mcSource, uint32_t* puiProcessedWordCount)
{
    MarkovWordSequence mwCurrentSequence(uiDepth);
    MarkovNode* pmnCurrentNode = nullptr;
    if (puiProcessedWordCount != nullptr)
    {
        *puiProcessedWordCount = 0;
    }
    while (true)
    {
        /* Find or create node(s) for current sequence */
        uint32_t uiWordCount = mwCurrentSequence.getWordCount();
        if (uiWordCount > 0)
        {
            pmnCurrentNode = &mnTreeRoot;
            for (uint32_t i = 0; i < uiWordCount; ++i)
            {
                pmnCurrentNode = &pmnCurrentNode->resolveChild(mcWordCollection.resolveWord(mwCurrentSequence.getWords()[i]));
            }
        }
        /* Fetch and prepare the next word and resulting sequence */
        string sNextWord = mcSource.getNextWord();
        if (sNextWord.empty())
        {
            break;
        }
        mwCurrentSequence.addOrShiftWord(sNextWord, uiDepth);
        /* Add next word as candidate for current node */
        if (pmnCurrentNode != nullptr)
        {
            pmnCurrentNode->addCandidate(mcWordCollection.resolveWord(sNextWord));
        }
        if (puiProcessedWordCount != nullptr)
        {
            ++*puiProcessedWordCount;
        }
    }
}

/**
 * Get the current state of the chain and bring the chain into the next state.
 * @param[out] pbResetOccurred set to true if the chain had to be randomly
 *                             re-initialized, set to false otherwise.
 * @return word associated with the current state
 */
string MarkovChain::generateWord(bool* pbResetOccurred)
{
    if (pbResetOccurred != nullptr)
    {
        *pbResetOccurred = false;
    }
    uint32_t uiWordCount = msCurrentState.getWordCount();
    if (uiWordCount < 1)
    {
        reset();
        uiWordCount = msCurrentState.getWordCount();
    }
    string sGeneratedWord = msCurrentState.getLastWord();

    const MarkovNode* pmnCurrentNode = &mnTreeRoot;
    for (uint32_t i = 0; i < uiWordCount; ++i)
    {
        if (pmnCurrentNode != nullptr)
        {
            const MarkovNode* pmnNewNode = pmnCurrentNode->findChild(mcWordCollection.resolveWord(msCurrentState.getWords()[i]));
            if (pmnNewNode == nullptr)
            {
                break;
            }
            else
            {
                pmnCurrentNode = pmnNewNode;
            }
        }
    }
    const string* psSelectedCandidate = nullptr;
    switch (mmOutputMode)
    {
    case MARKOV_MODE_RANDOM:
        psSelectedCandidate = pmnCurrentNode->getRandomCandidate(mtRandomSource);
        break;
    case MARKOV_MODE_PROBABLE:
        // To be implemented at a later time
        //psSelectedCandidate = pmnCurrentNode->getProbableCandidate(mtRandomSource);
        break;
    }
    if (psSelectedCandidate == nullptr || pmnCurrentNode == nullptr)
    {
        if (pbResetOccurred != nullptr)
        {
            *pbResetOccurred = true;
        }
        if (mmOutputMode == MARKOV_MODE_RANDOM)
        {
            reset();
            return sGeneratedWord;
        }
        else
        {
            return "";
        }
    }
    msCurrentState.addOrShiftWord(*psSelectedCandidate, uiDepth);
    return sGeneratedWord;
}

/**
 * Scale a 32-bit unsigned integer value. Useful for RNG output.
 * @param[in] uiValue integer to be scaled
 * @param[in] uiMin minimum output value, inclusive
 * @param[in] uiMax maximum output value, inclusive
 * @return scaled integer
 */
uint32_t fitValueWithinRange(uint32_t uiValue, uint32_t uiMin, uint32_t uiMax)
{
    double dScale = (double) uiValue / UINT32_MAX;
    double dMagnitude = dScale * (uiMax - uiMin);
    uint32_t uiResult = round(dMagnitude + uiMin);
    if (uiResult > uiMax)
    {
        return uiMax;
    }
    if (uiResult < uiMin)
    {
        return uiMin;
    }
    return uiResult;
}

MarkovException::MarkovException()
    : runtime_error("")
{
}

MarkovException::MarkovException(string sDescription)
    : runtime_error(sDescription)
{
}
