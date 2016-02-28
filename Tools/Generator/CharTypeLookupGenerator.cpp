/*
 * Copyright (C) 2016 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Utilities/Math.h"
#include "Utilities/Converter.h"
#include "Utilities/ArrayBuilder.h"
#include "Utilities/FileBuilder.h"

#include "CharTypeLookupGenerator.h"

using namespace std;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Generator;
using namespace SheenBidi::Generator::Utilities;

static const size_t MIN_MAIN_SEGMENT_SIZE = 32;
static const size_t MAX_MAIN_SEGMENT_SIZE = 512;

static const size_t MIN_BRANCH_SEGMENT_SIZE = 32;
static const size_t MAX_BRANCH_SEGMENT_SIZE = 512;

static const string DATA_ARRAY_TYPE = "static const SBUInt8";
static const string DATA_ARRAY_NAME = "_SBCharTypePrimaryData";

static const string MAIN_INDEXES_ARRAY_TYPE = "static const SBUInt16";
static const string MAIN_INDEXES_ARRAY_NAME = "_SBCharTypeMainIndexes";

static const string BRANCH_INDEXES_ARRAY_TYPE = "static const SBUInt16";
static const string BRANCH_INDEXES_ARRAY_NAME = "_SBCharTypeBranchIndexes";

CharTypeLookupGenerator::MainDataSegment::MainDataSegment(size_t index, MainDataSet dataset)
    : index(index)
    , dataset(dataset)
{
}

const string CharTypeLookupGenerator::MainDataSegment::macroLine() const {
    return ("#define " + macroName() + " 0x" + Converter::toHex(index, 4));
}

const string CharTypeLookupGenerator::MainDataSegment::macroName() const {
    return (DATA_ARRAY_NAME + "_" + Converter::toHex(index, 4));
}

CharTypeLookupGenerator::BranchDataSegment::BranchDataSegment(size_t index, BranchDataSet dataset)
    : index(index)
    , dataset(dataset)
{
}

const string CharTypeLookupGenerator::BranchDataSegment::macroLine() const {
    return ("#define " + macroName() + " 0x" + Converter::toHex(index, 4));
}

const string CharTypeLookupGenerator::BranchDataSegment::macroName() const {
    return (MAIN_INDEXES_ARRAY_NAME + "_" + Converter::toHex(index, 4));
}

CharTypeLookupGenerator::CharTypeLookupGenerator(const UnicodeData &unicodeData)
    : m_bidiClassDetector(unicodeData)
    , m_firstCodePoint(unicodeData.firstCodePoint())
    , m_lastCodePoint(unicodeData.lastCodePoint())
    , m_mainSegmentSize(0)
    , m_branchSegmentSize(0)
{
}

void CharTypeLookupGenerator::setMainSegmentSize(size_t segmentSize) {
    m_mainSegmentSize = min(MAX_MAIN_SEGMENT_SIZE, max(MIN_MAIN_SEGMENT_SIZE, segmentSize));
}

void CharTypeLookupGenerator::setBranchSegmentSize(size_t segmentSize) {
    m_branchSegmentSize = min(MAX_BRANCH_SEGMENT_SIZE, max(MIN_BRANCH_SEGMENT_SIZE, segmentSize));
}

void CharTypeLookupGenerator::displayBidiClassesFrequency() {
    map<uint8_t, size_t> frequency;

    for (uint32_t i = 0; i < m_lastCodePoint; i++) {
        frequency[m_bidiClassDetector.numberForCodePoint(i)]++;
    }

    for (auto &element : frequency) {
        cout << m_bidiClassDetector.numberToName(element.first) << " Code Points: " << element.second << endl;
    }
}

void CharTypeLookupGenerator::analyzeData() {
    cout << "Analyzing data for char type lookup." << endl;

    size_t memory = analyzeData(true);

    cout << "  Main Segment Size: " << m_mainSegmentSize << endl;
    cout << "  Branch Segment Size: " << m_branchSegmentSize << endl;
    cout << "  Required Memory: " << memory << " bytes";
}

size_t CharTypeLookupGenerator::analyzeData(bool all) {
    size_t minMemory = SIZE_MAX;
    size_t dataSize = 0;

    if (all || m_mainSegmentSize == 0) {
        size_t minSize = SIZE_MAX;
        minMemory = SIZE_MAX;

        m_mainSegmentSize = MIN_MAIN_SEGMENT_SIZE;
        while (m_mainSegmentSize <= MAX_MAIN_SEGMENT_SIZE) {
            collectMainData();

            size_t memory = m_dataSize + (m_mainIndexesSize * 2);
            if (memory < minMemory) {
                minSize = m_mainSegmentSize;
                minMemory = memory;
                dataSize = m_dataSize;
            }

            m_mainSegmentSize++;
        }
        m_mainSegmentSize = minSize;
        collectMainData();
    }
    
    if (all || m_branchSegmentSize == 0) {
        size_t minSize = SIZE_MAX;
        minMemory = SIZE_MAX;

        m_branchSegmentSize = MIN_BRANCH_SEGMENT_SIZE;
        while (m_branchSegmentSize <= MAX_BRANCH_SEGMENT_SIZE) {
            collectBranchData();

            size_t branchBytes = m_branchSegments.at(m_branchSegments.size() - 1).index > UINT8_MAX ? 2 : 1;
            size_t memory = (m_mainIndexesSize * 2) + (m_branchIndexesSize * branchBytes);
            if (memory < minMemory) {
                minSize = m_branchSegmentSize;
                minMemory = memory;
            }

            m_branchSegmentSize++;
        }
        m_branchSegmentSize = minSize;
        collectBranchData();
    }

    if (all) {
        return (dataSize + minMemory);
    }

    return 0;
}

void CharTypeLookupGenerator::collectMainData() {
    size_t unicodeCount = m_lastCodePoint - m_firstCodePoint;
    size_t maxSegments = Math::FastCeil(unicodeCount, m_mainSegmentSize);

    m_dataSegments.clear();
    m_dataSegments.reserve(maxSegments);

    m_dataReferences.clear();
    m_dataReferences.reserve(maxSegments);

    m_dataSize = 0;

    uint8_t defaultBidiClass = m_bidiClassDetector.nameToNumber("L");

    for (size_t i = 0; i < maxSegments; i++) {
        uint32_t segmentStart = m_firstCodePoint + (uint32_t)(i * m_mainSegmentSize);
        uint32_t segmentEnd = min(m_lastCodePoint, (uint32_t)(segmentStart + m_mainSegmentSize - 1));

        MainDataSet dataset(new UnsafeMainDataSet());
        dataset->reserve(m_mainSegmentSize);

        for (uint32_t unicode = segmentStart; unicode <= segmentEnd; unicode++) {
            uint8_t number = m_bidiClassDetector.numberForCodePoint(unicode);
            if (number) {
                dataset->push_back(number);
            } else {
                dataset->push_back(defaultBidiClass);
            }
        }

        size_t segmentIndex = SIZE_MAX;
        size_t segmentCount = m_dataSegments.size();
        for (size_t j = 0; j < segmentCount; j++) {
            if (*m_dataSegments[j].dataset == *dataset) {
                segmentIndex = j;
            }
        }

        if (segmentIndex == SIZE_MAX) {
            segmentIndex = m_dataSegments.size();
            m_dataSegments.push_back(MainDataSegment(m_dataSize, dataset));
            m_dataSize += dataset->size();
        }

        m_dataReferences.push_back(&m_dataSegments.at(segmentIndex));
    }

    m_mainIndexesSize = m_dataReferences.size();
}

void CharTypeLookupGenerator::collectBranchData() {
    size_t mainIndexesCount = m_dataReferences.size() - 1;
    size_t maxSegments = Math::FastCeil(mainIndexesCount, m_branchSegmentSize);

    m_branchSegments.clear();
    m_branchSegments.reserve(maxSegments);

    m_branchReferences.clear();
    m_branchReferences.reserve(maxSegments);

    m_mainIndexesSize = 0;

    for (size_t i = 0; i < maxSegments; i++) {
        size_t segmentStart = (i * m_branchSegmentSize);
        size_t segmentEnd = min(mainIndexesCount, segmentStart + m_branchSegmentSize - 1);

        BranchDataSet dataset(new UnsafeBranchDataSet());
        dataset->reserve(m_branchSegmentSize);

        for (size_t i = segmentStart; i <= segmentEnd; i++) {
            dataset->push_back(m_dataReferences.at(i));
        }

        size_t segmentIndex = SIZE_MAX;
        size_t segmentCount = m_branchSegments.size();
        for (size_t j = 0; j < segmentCount; j++) {
            if (*m_branchSegments[j].dataset == *dataset) {
                segmentIndex = j;
            }
        }

        if (segmentIndex == SIZE_MAX) {
            segmentIndex = m_branchSegments.size();
            m_branchSegments.push_back(BranchDataSegment(m_mainIndexesSize, dataset));
            m_mainIndexesSize += dataset->size();
        }

        m_branchReferences.push_back(&m_branchSegments.at(segmentIndex));
    }
    
    m_branchIndexesSize = m_branchReferences.size();
}

void CharTypeLookupGenerator::generateFile(const std::string &directory) {
    analyzeData(false);
    collectMainData();
    collectBranchData();

    std::set<string> charTypes;

    ArrayBuilder arrData;
    arrData.setDataType(DATA_ARRAY_TYPE);
    arrData.setName(DATA_ARRAY_NAME);
    arrData.setElementSpace(3);

    auto dataPtr = m_dataSegments.begin();
    auto dataEnd = m_dataSegments.end();
    for (; dataPtr != dataEnd; dataPtr++) {
        const MainDataSegment &segment = *dataPtr;
        bool isLast = (dataPtr == (dataEnd - 1));

        arrData.append(segment.macroLine());
        arrData.newLine();

        size_t length = segment.dataset->size();

        for (size_t j = 0; j < length; j++) {
            const string &charType = m_bidiClassDetector.numberToName(segment.dataset->at(j));
            charTypes.insert(charType);
            arrData.appendElement(charType);

            if (!isLast || j != (length - 1)) {
                arrData.newElement();
            }
        }

        if (!isLast) {
            arrData.newLine();
        }
    }

    ArrayBuilder arrMainIndexes;
    arrMainIndexes.setDataType(MAIN_INDEXES_ARRAY_TYPE);
    arrMainIndexes.setName(MAIN_INDEXES_ARRAY_NAME);

    size_t segmentStart = m_firstCodePoint;
    auto mainIndexPtr = m_branchSegments.begin();
    auto mainIndexEnd = m_branchSegments.end();
    for (; mainIndexPtr != mainIndexEnd; mainIndexPtr++) {
        const BranchDataSegment &segment = *mainIndexPtr;
        bool isLast = (mainIndexPtr == (mainIndexEnd - 1));

        arrMainIndexes.append(segment.macroLine());
        arrMainIndexes.newLine();

        size_t length = segment.dataset->size();

        for (size_t j = 0; j < length; j++) {
            arrMainIndexes.appendElement(segment.dataset->at(j)->macroName());

            if (!isLast || j != (length - 1)) {
                arrMainIndexes.newElement();
                arrMainIndexes.newLine();
            }
        }

        segmentStart += m_mainSegmentSize;
    }

    ArrayBuilder arrBranchIndexes;
    arrBranchIndexes.setDataType(BRANCH_INDEXES_ARRAY_TYPE);
    arrBranchIndexes.setName(BRANCH_INDEXES_ARRAY_NAME);

    segmentStart = 0;
    auto branchIndexPtr = m_branchReferences.begin();
    auto branchIndexEnd = m_branchReferences.end();
    for (; branchIndexPtr != branchIndexEnd; branchIndexPtr++) {
        const BranchDataSegment &segment = **branchIndexPtr;
        bool isLast = (branchIndexPtr == (branchIndexEnd - 1));

        arrBranchIndexes.appendElement(segment.macroName());
        if (!isLast) {
            arrBranchIndexes.newElement();
            arrBranchIndexes.newLine();
        }

        segmentStart += m_branchSegmentSize;
    }

    TextBuilder lookupFunction;
    string maxUnicode = "0x" + Converter::toHex(m_lastCodePoint, 6);
    string mainDivider = "0x" + Converter::toHex(m_mainSegmentSize, 4);
    string branchDivider = "0x" + Converter::toHex(m_mainSegmentSize * m_branchSegmentSize, 4);
    lookupFunction.append("SB_INTERNAL SBCharType SBCharTypeDetermine(SBCodepoint codepoint)").newLine();
    lookupFunction.append("{").newLine();
    lookupFunction.appendTabs(1).append("if (codepoint <= " + maxUnicode + ") {").newLine();
    lookupFunction.appendTabs(2).append("return _SBCharTypePrimaryData[").newLine();
    lookupFunction.appendTabs(2).append("        _SBCharTypeMainIndexes[").newLine();
    lookupFunction.appendTabs(2).append("         _SBCharTypeBranchIndexes[").newLine();
    lookupFunction.appendTabs(2).append("              codepoint / " + branchDivider).newLine();
    lookupFunction.appendTabs(2).append("         ] + (codepoint % " + branchDivider + ") / " + mainDivider).newLine();
    lookupFunction.appendTabs(2).append("        ] + codepoint % " + mainDivider).newLine();
    lookupFunction.appendTabs(2).append("       ];").newLine();
    lookupFunction.appendTabs(1).append("}").newLine();
    lookupFunction.newLine();
    lookupFunction.appendTabs(1).append("return SBCharTypeL;").newLine();
    lookupFunction.append("}").newLine();

    FileBuilder header(directory + "/SBCharTypeLookup.h");
    header.append("/*").newLine();
    header.append(" * Automatically generated by SheenBidiGenerator tool.").newLine();
    header.append(" * DO NOT EDIT!!").newLine();
    header.append(" */").newLine();
    header.newLine();
    header.append("#ifndef _SB_INTERNAL_CHAR_TYPE_LOOKUP_H").newLine();
    header.append("#define _SB_INTERNAL_CHAR_TYPE_LOOKUP_H").newLine();
    header.newLine();
    header.append("#include <SBConfig.h>").newLine();
    header.newLine();
    header.append("#include \"SBCharType.h\"").newLine();
    header.append("#include \"SBTypes.h\"").newLine();
    header.newLine();
    header.append("SB_INTERNAL SBCharType SBCharTypeDetermine(SBCodepoint codepoint);").newLine();
    header.newLine();
    header.append("#endif").newLine();

    FileBuilder source(directory + "/SBCharTypeLookup.c");
    source.append("/*").newLine();
    source.append(" * Automatically generated by SheenBidiGenerator tool.").newLine();
    source.append(" * DO NOT EDIT!!").newLine();
    source.append(" */").newLine();
    source.newLine();
    source.append("#include \"SBCharTypeLookup.h\"").newLine();
    source.newLine();
    for (auto ct : charTypes) {
        string str = ct + string(3 - ct.length(), ' ');
        source.append("#define " + str).appendTab().append(" SBCharType" + str).newLine();
    }
    source.newLine();
    source.append(arrData).newLine();
    source.append(arrMainIndexes).newLine();
    source.append(arrBranchIndexes).newLine();
    source.append(lookupFunction).newLine();
    for (auto ct : charTypes) {
        source.append("#undef " + ct).newLine();
    }
}
