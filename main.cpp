#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <queue>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <cstdio>

using namespace std;

// ============================================================
// FILE ARCHIVER AND COMPRESSOR
// C++11 compatible
//
// This version creates a real .huff archive and can extract it.
//
// Archive format used by this educational prototype:
//   MAGIC: 4 bytes "HUF1"
//   ORIGINAL_SIZE: uint64
//   UNIQUE_SYMBOLS: uint16
//   For each symbol:
//       SYMBOL: 1 byte
//       FREQUENCY: uint64
//   ENCODED_BIT_COUNT: uint64
//   ENCODED_DATA: packed bits, MSB first
//
// Storing frequencies allows the Huffman tree to be reconstructed
// during extraction.
// ============================================================

struct HuffmanNode {
    unsigned char data;
    unsigned long long frequency;
    HuffmanNode* left;
    HuffmanNode* right;

    HuffmanNode(unsigned char d, unsigned long long f)
        : data(d), frequency(f), left(NULL), right(NULL) {}

    HuffmanNode(unsigned long long f, HuffmanNode* l, HuffmanNode* r)
        : data(0), frequency(f), left(l), right(r) {}

    bool isLeaf() const {
        return left == NULL && right == NULL;
    }
};

struct CompareNodes {
    bool operator()(HuffmanNode* a, HuffmanNode* b) const {
        if (a->frequency != b->frequency)
            return a->frequency > b->frequency;
        return a->data > b->data;
    }
};

class HuffmanCompressor {
private:
    map<unsigned char, unsigned long long> frequencies;
    map<unsigned char, string> codes;
    HuffmanNode* root;

    void deleteTree(HuffmanNode* node) {
        if (!node) return;
        deleteTree(node->left);
        deleteTree(node->right);
        delete node;
    }

    void generateCodes(HuffmanNode* node, const string& code) {
        if (!node) return;

        if (node->isLeaf()) {
            codes[node->data] = code.empty() ? "0" : code;
            return;
        }

        generateCodes(node->left, code + "0");
        generateCodes(node->right, code + "1");
    }

    string printable(unsigned char c) const {
        if (c == '\n') return "\\n";
        if (c == '\r') return "\\r";
        if (c == '\t') return "\\t";
        if (c == ' ') return "[space]";
        if (isprint(c)) return string(1, (char)c);

        stringstream ss;
        ss << "ASCII " << (int)c;
        return ss.str();
    }

    void writeU16(ofstream& out, unsigned short value) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    void writeU64(ofstream& out, unsigned long long value) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    bool readU16(ifstream& in, unsigned short& value) {
        in.read(reinterpret_cast<char*>(&value), sizeof(value));
        return in.good();
    }

    bool readU64(ifstream& in, unsigned long long& value) {
        in.read(reinterpret_cast<char*>(&value), sizeof(value));
        return in.good();
    }

    void buildTreeFromFrequencies(
        const map<unsigned char, unsigned long long>& freq) {

        deleteTree(root);
        root = NULL;

        priority_queue<HuffmanNode*,
                       vector<HuffmanNode*>,
                       CompareNodes> heap;

        map<unsigned char, unsigned long long>::const_iterator it;

        for (it = freq.begin(); it != freq.end(); ++it) {
            heap.push(new HuffmanNode(it->first, it->second));
        }

        if (heap.empty()) return;

        if (heap.size() == 1) {
            root = heap.top();
            heap.pop();
            return;
        }

        while (heap.size() > 1) {
            HuffmanNode* a = heap.top();
            heap.pop();

            HuffmanNode* b = heap.top();
            heap.pop();

            heap.push(new HuffmanNode(
                a->frequency + b->frequency, a, b));
        }

        root = heap.top();
        heap.pop();
    }

public:
    HuffmanCompressor() : root(NULL) {}

    ~HuffmanCompressor() {
        deleteTree(root);
    }

    bool readInput(const string& filename, string& data) {
        ifstream in(filename.c_str(), ios::binary);
        if (!in) return false;

        data.clear();
        char ch;

        while (in.get(ch))
            data += ch;

        in.close();

        return !data.empty();
    }

    void calculateFrequencies(const string& data) {
        frequencies.clear();

        for (size_t i = 0; i < data.size(); ++i) {
            unsigned char c = (unsigned char)data[i];
            frequencies[c]++;
        }
    }

    void buildTree() {
        buildTreeFromFrequencies(frequencies);
    }

    void generateCodes() {
        codes.clear();
        generateCodes(root, "");
    }

    void printFrequencyTable() const {
        cout << "\n============================================================\n";
        cout << "                    FREQUENCY TABLE\n";
        cout << "============================================================\n";
        cout << "| Character            | Frequency |\n";
        cout << "------------------------------------------------------------\n";

        map<unsigned char, unsigned long long>::const_iterator it;

        for (it = frequencies.begin(); it != frequencies.end(); ++it) {
            cout << "| " << left << setw(20) << printable(it->first)
                 << " | " << right << setw(9) << it->second << " |\n";
        }

        cout << "============================================================\n";
    }

    void printCodes() const {
        cout << "\n============================================================\n";
        cout << "                     HUFFMAN CODES\n";
        cout << "============================================================\n";
        cout << "| Character            | Code                 |\n";
        cout << "------------------------------------------------------------\n";

        map<unsigned char, string>::const_iterator it;

        for (it = codes.begin(); it != codes.end(); ++it) {
            cout << "| " << left << setw(20) << printable(it->first)
                 << " | " << setw(20) << it->second << " |\n";
        }

        cout << "============================================================\n";
    }

    void printTree(HuffmanNode* node,
                   const string& indent,
                   const string& branch) const {
        if (!node) return;

        cout << indent << branch;

        if (node->isLeaf()) {
            cout << "[" << printable(node->data)
                 << " : " << node->frequency << "]\n";
        } else {
            cout << "(internal : " << node->frequency << ")\n";
        }

        if (node->left)
            printTree(node->left, indent + "    ", "0 -> ");

        if (node->right)
            printTree(node->right, indent + "    ", "1 -> ");
    }

    void printTree() const {
        cout << "\n============================================================\n";
        cout << "                    HUFFMAN BINARY TREE\n";
        cout << "============================================================\n";
        cout << "0 = LEFT, 1 = RIGHT\n\n";

        printTree(root, "", "ROOT -> ");

        cout << "============================================================\n";
    }

    string encodeToBits(const string& data) const {
        string bits;

        for (size_t i = 0; i < data.size(); ++i)
            bits += codes.find((unsigned char)data[i])->second;

        return bits;
    }

    // --------------------------------------------------------
    // Compress and save a real .huff archive
    // --------------------------------------------------------
    bool compressFile(const string& inputFile,
                      const string& archiveFile) {

        string data;

        if (!readInput(inputFile, data)) {
            cout << "\nERROR: Could not read input file.\n";
            return false;
        }

        calculateFrequencies(data);
        buildTree();
        generateCodes();

        string bits = encodeToBits(data);

        ofstream out(archiveFile.c_str(), ios::binary);

        if (!out) {
            cout << "\nERROR: Could not create archive file.\n";
            return false;
        }

        // Magic number
        const char magic[4] = {'H', 'U', 'F', '1'};
        out.write(magic, 4);

        // Original file size
        writeU64(out, (unsigned long long)data.size());

        // Number of unique symbols
        writeU16(out, (unsigned short)frequencies.size());

        // Frequency table
        map<unsigned char, unsigned long long>::const_iterator it;

        for (it = frequencies.begin(); it != frequencies.end(); ++it) {
            unsigned char symbol = it->first;
            out.write(reinterpret_cast<const char*>(&symbol), 1);
            writeU64(out, it->second);
        }

        // Number of valid encoded bits
        writeU64(out, (unsigned long long)bits.size());

        // Pack bits into bytes
        unsigned char currentByte = 0;
        int bitCount = 0;

        for (size_t i = 0; i < bits.size(); ++i) {
            currentByte <<= 1;

            if (bits[i] == '1')
                currentByte |= 1;

            bitCount++;

            if (bitCount == 8) {
                out.write(reinterpret_cast<const char*>(&currentByte), 1);
                currentByte = 0;
                bitCount = 0;
            }
        }

        if (bitCount != 0) {
            currentByte <<= (8 - bitCount);
            out.write(reinterpret_cast<const char*>(&currentByte), 1);
        }

        out.close();

        return true;
    }

    // --------------------------------------------------------
    // Extract a .huff archive
    // --------------------------------------------------------
    bool extractFile(const string& archiveFile,
                     const string& outputFile) {

        ifstream in(archiveFile.c_str(), ios::binary);

        if (!in) {
            cout << "\nERROR: Could not open archive.\n";
            return false;
        }

        char magic[4];
        in.read(magic, 4);

        if (!in || magic[0] != 'H' || magic[1] != 'U' ||
            magic[2] != 'F' || magic[3] != '1') {

            cout << "\nERROR: Invalid .huff archive.\n";
            in.close();
            return false;
        }

        unsigned long long originalSize = 0;
        unsigned short uniqueSymbols = 0;

        if (!readU64(in, originalSize) ||
            !readU16(in, uniqueSymbols)) {

            in.close();
            return false;
        }

        map<unsigned char, unsigned long long> storedFrequencies;

        for (unsigned int i = 0; i < uniqueSymbols; ++i) {
            unsigned char symbol;
            unsigned long long frequency;

            in.read(reinterpret_cast<char*>(&symbol), 1);

            if (!in || !readU64(in, frequency)) {
                in.close();
                return false;
            }

            storedFrequencies[symbol] = frequency;
        }

        unsigned long long bitCount = 0;

        if (!readU64(in, bitCount)) {
            in.close();
            return false;
        }

        buildTreeFromFrequencies(storedFrequencies);

        string result;
        result.reserve((size_t)originalSize);

        // Single-symbol case
        if (root != NULL && root->isLeaf()) {
            for (unsigned long long i = 0; i < originalSize; ++i)
                result += (char)root->data;

            in.close();

            ofstream out(outputFile.c_str(), ios::binary);
            if (!out) return false;

            out.write(result.data(), (streamsize)result.size());
            out.close();

            return true;
        }

        HuffmanNode* current = root;
        unsigned long long bitsRead = 0;

        while (bitsRead < bitCount) {
            unsigned char byte;

            in.read(reinterpret_cast<char*>(&byte), 1);

            if (!in) {
                in.close();
                return false;
            }

            for (int bit = 7; bit >= 0 && bitsRead < bitCount; --bit) {
                int value = (byte >> bit) & 1;

                if (value == 0)
                    current = current->left;
                else
                    current = current->right;

                // Count this bit immediately after reading it.
                ++bitsRead;

                if (current == NULL) {
                    in.close();
                    return false;
                }

                if (current->isLeaf()) {
                    result += (char)current->data;
                    current = root;

                    if (result.size() == (size_t)originalSize)
                        break;
                }
            }
        }

        in.close();

        if (result.size() != (size_t)originalSize) {
            cout << "\nERROR: Extracted size does not match original size.\n";
            return false;
        }

        ofstream out(outputFile.c_str(), ios::binary);

        if (!out) return false;

        out.write(result.data(), (streamsize)result.size());
        out.close();

        return true;
    }

    void showCompressionStatistics(
        const string& inputFile,
        const string& archiveFile) {

        ifstream original(inputFile.c_str(), ios::binary);
        ifstream archive(archiveFile.c_str(), ios::binary);

        original.seekg(0, ios::end);
        archive.seekg(0, ios::end);

        long long originalSize = (long long)original.tellg();
        long long archiveSize = (long long)archive.tellg();

        original.close();
        archive.close();

        double ratio = 0.0;
        double saving = 0.0;

        if (originalSize > 0) {
            ratio = (double)archiveSize / originalSize;
            saving = (1.0 - ratio) * 100.0;
        }

        cout << "\n============================================================\n";
        cout << "                 COMPRESSION STATISTICS\n";
        cout << "============================================================\n";
        cout << "Original file size  : " << originalSize << " bytes\n";
        cout << "Archive file size   : " << archiveSize << " bytes\n";
        cout << fixed << setprecision(2);
        cout << "Archive ratio       : " << ratio << "\n";
        cout << "Archive size change : " << saving << "%\n";
        cout << "(Archive includes Huffman metadata/tree information.)\n";
        cout << "============================================================\n";
    }
};

// ============================================================
// Main
// ============================================================
int main() {

    cout << "\n============================================================\n";
    cout << "              FILE ARCHIVER AND COMPRESSOR\n";
    cout << "============================================================\n";

    cout << "\n1. Compress a file\n";
    cout << "2. Extract a .huff archive\n";
    cout << "3. Run complete demo\n";
    cout << "\nEnter choice: ";

    int choice;
    cin >> choice;
    cin.ignore(10000, '\n');

    HuffmanCompressor compressor;

    // --------------------------------------------------------
    // COMPRESS
    // --------------------------------------------------------
    if (choice == 1 || choice == 3) {

        string inputFile;
        string archiveFile;

        if (choice == 3) {
            inputFile = "sample.txt";
            archiveFile = "sample.huff";
        } else {
            cout << "Enter input filename: ";
            getline(cin, inputFile);

            cout << "Enter archive filename "
                 << "(press Enter for output.huff): ";

            getline(cin, archiveFile);

            if (archiveFile.empty())
                archiveFile = "output.huff";
        }

        cout << "\n[1] Reading and analyzing file...\n";

        string dummy;

        if (!compressor.readInput(inputFile, dummy)) {
            cout << "ERROR: Cannot open/read " << inputFile << "\n";
            return 1;
        }

        compressor.calculateFrequencies(dummy);

        cout << "[2] Frequency analysis complete.\n";
        compressor.printFrequencyTable();

        cout << "\n[3] Building Min Priority Queue...\n";
        compressor.buildTree();

        cout << "[4] Huffman Binary Tree constructed.\n";
        compressor.printTree();

        cout << "\n[5] Generating Huffman Codes...\n";
        compressor.generateCodes();
        compressor.printCodes();

        cout << "\n[6] Compressing and creating archive...\n";

        if (!compressor.compressFile(inputFile, archiveFile)) {
            cout << "ERROR: Compression failed.\n";
            return 1;
        }

        cout << "\nSUCCESS: Compressed archive created:\n";
        cout << "         " << archiveFile << "\n";

        compressor.showCompressionStatistics(inputFile, archiveFile);
    }

    // --------------------------------------------------------
    // EXTRACT
    // --------------------------------------------------------
    if (choice == 2 || choice == 3) {

        string archiveFile;
        string outputFile;

        if (choice == 3) {
            archiveFile = "sample.huff";
            outputFile = "sample_extracted.txt";
        } else {
            cout << "\nEnter .huff archive filename: ";
            getline(cin, archiveFile);

            cout << "Enter output filename "
                 << "(press Enter for extracted.txt): ";

            getline(cin, outputFile);

            if (outputFile.empty())
                outputFile = "extracted.txt";
        }

        cout << "\n[7] Extracting archive...\n";

        if (!compressor.extractFile(archiveFile, outputFile)) {
            cout << "ERROR: Extraction failed.\n";
            return 1;
        }

        cout << "\nSUCCESS: File extracted:\n";
        cout << "         " << outputFile << "\n";

        // Verify original and extracted file for demo mode
        if (choice == 3) {
            ifstream a("sample.txt", ios::binary);
            ifstream b("sample_extracted.txt", ios::binary);

            string original(
                (istreambuf_iterator<char>(a)),
                istreambuf_iterator<char>());

            string extracted(
                (istreambuf_iterator<char>(b)),
                istreambuf_iterator<char>());

            a.close();
            b.close();

            cout << "\n============================================================\n";

            if (original == extracted) {
                cout << "LOSSLESS VERIFICATION: SUCCESS\n";
                cout << "Original and extracted files are IDENTICAL.\n";
            } else {
                cout << "LOSSLESS VERIFICATION: FAILED\n";
            }

            cout << "============================================================\n";
        }
    }

    cout << "\nProject files are saved in the same folder where\n";
    cout << "you ran FileArchiver.exe.\n";

    cout << "\nPress Enter to exit...";
    cin.get();

    return 0;
}
