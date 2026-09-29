#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BUFFER_SIZE 65536
#define MAX_TREE_NODES 512
#define MAX_CODE_LEN 256

// ============================================================
// Data Structures: Node & Min-Heap
// ============================================================
typedef struct HuffmanNode {
    uint8_t data;
    uint64_t frequency;
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

typedef struct {
    HuffmanNode* nodes[MAX_TREE_NODES];
    int size;
} MinHeap;

HuffmanNode* create_node(uint8_t data, uint64_t freq, HuffmanNode* left, HuffmanNode* right) {
    HuffmanNode* node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    if (!node) {
        fprintf(stderr, "Memory allocation error.\n");
        exit(EXIT_FAILURE);
    }
    node->data = data;
    node->frequency = freq;
    node->left = left;
    node->right = right;
    return node;
}

void heap_init(MinHeap* h) {
    h->size = 0;
}

void heap_push(MinHeap* h, HuffmanNode* node) {
    int i = h->size++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->nodes[parent]->frequency <= node->frequency)
            break;
        h->nodes[i] = h->nodes[parent];
        i = parent;
    }
    h->nodes[i] = node;
}

HuffmanNode* heap_pop(MinHeap* h) {
    if (h->size <= 0) return NULL;
    HuffmanNode* root = h->nodes[0];
    HuffmanNode* last = h->nodes[--h->size];

    int i = 0;
    while (i * 2 + 1 < h->size) {
        int left = i * 2 + 1;
        int right = i * 2 + 2;
        int smallest = left;

        if (right < h->size && h->nodes[right]->frequency < h->nodes[left]->frequency) {
            smallest = right;
        }

        if (last->frequency <= h->nodes[smallest]->frequency) {
            break;
        }

        h->nodes[i] = h->nodes[smallest];
        i = smallest;
    }
    h->nodes[i] = last;
    return root;
}

void free_tree(HuffmanNode* node) {
    if (!node) return;
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

// ============================================================
// Bit-Level Stream Handlers
// ============================================================
typedef struct {
    FILE* out;
    uint8_t buffer;
    uint8_t bit_count;
} BitWriter;

void bit_writer_init(BitWriter* bw, FILE* out) {
    bw->out = out;
    bw->buffer = 0;
    bw->bit_count = 0;
}

void bit_writer_write(BitWriter* bw, const char* code) {
    for (int i = 0; code[i] != '\0'; ++i) {
        bw->buffer <<= 1;
        if (code[i] == '1') {
            bw->buffer |= 1;
        }
        bw->bit_count++;

        if (bw->bit_count == 8) {
            fputc(bw->buffer, bw->out);
            bw->buffer = 0;
            bw->bit_count = 0;
        }
    }
}

void bit_writer_flush(BitWriter* bw) {
    if (bw->bit_count > 0) {
        bw->buffer <<= (8 - bw->bit_count);
        fputc(bw->buffer, bw->out);
        bw->buffer = 0;
        bw->bit_count = 0;
    }
}

typedef struct {
    FILE* in;
    uint8_t buffer;
    uint8_t bit_count;
} BitReader;

void bit_reader_init(BitReader* br, FILE* in) {
    br->in = in;
    br->buffer = 0;
    br->bit_count = 0;
}

int bit_reader_read_bit(BitReader* br) {
    if (br->bit_count == 0) {
        int ch = fgetc(br->in);
        if (ch == EOF) return -1;
        br->buffer = (uint8_t)ch;
        br->bit_count = 8;
    }
    br->bit_count--;
    return (br->buffer >> br->bit_count) & 1;
}

// ============================================================
// Tree Construction & Code Generation
// ============================================================
void generate_codes(HuffmanNode* node, char* current_code, int depth, char codes[256][MAX_CODE_LEN]) {
    if (!node) return;

    if (node->left == NULL && node->right == NULL) {
        if (depth == 0) {
            codes[node->data][0] = '0';
            codes[node->data][1] = '\0';
        } else {
            current_code[depth] = '\0';
            strcpy(codes[node->data], current_code);
        }
        return;
    }

    current_code[depth] = '0';
    generate_codes(node->left, current_code, depth + 1, codes);

    current_code[depth] = '1';
    generate_codes(node->right, current_code, depth + 1, codes);
}

HuffmanNode* build_huffman_tree(const uint64_t frequencies[256]) {
    MinHeap heap;
    heap_init(&heap);

    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] > 0) {
            heap_push(&heap, create_node((uint8_t)i, frequencies[i], NULL, NULL));
        }
    }

    if (heap.size == 0) return NULL;
    if (heap.size == 1) return heap_pop(&heap);

    while (heap.size > 1) {
        HuffmanNode* left = heap_pop(&heap);
        HuffmanNode* right = heap_pop(&heap);
        HuffmanNode* parent = create_node(0, left->frequency + right->frequency, left, right);
        heap_push(&heap, parent);
    }

    return heap_pop(&heap);
}

const char* get_filename_only(const char* path) {
    const char* p1 = strrchr(path, '/');
    const char* p2 = strrchr(path, '\\');
    const char* res = path;
    if (p1 && p1 + 1 > res) res = p1 + 1;
    if (p2 && p2 + 1 > res) res = p2 + 1;
    return res;
}

// ============================================================
// Compression Implementation
// ============================================================
int compress_file(const char* input_path, const char* archive_path) {
    FILE* in = fopen(input_path, "rb");
    if (!in) {
        fprintf(stderr, "ERROR: Cannot open input file: %s\n", input_path);
        return 0;
    }

    uint64_t frequencies[256] = {0};
    uint8_t* buffer = (uint8_t*)malloc(BUFFER_SIZE);
    uint64_t total_bytes = 0;
    size_t bytes_read;

    // Pass 1: Frequency analysis
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, in)) > 0) {
        total_bytes += bytes_read;
        for (size_t i = 0; i < bytes_read; ++i) {
            frequencies[buffer[i]]++;
        }
    }

    if (total_bytes == 0) {
        fprintf(stderr, "ERROR: Input file is empty.\n");
        free(buffer);
        fclose(in);
        return 0;
    }

    HuffmanNode* root = build_huffman_tree(frequencies);
    char codes[256][MAX_CODE_LEN] = {{0}};
    char current_code[MAX_CODE_LEN] = {0};
    generate_codes(root, current_code, 0, codes);

    uint64_t total_encoded_bits = 0;
    uint16_t unique_symbols = 0;
    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] > 0) {
            unique_symbols++;
            total_encoded_bits += frequencies[i] * strlen(codes[i]);
        }
    }

    FILE* out = fopen(archive_path, "wb");
    if (!out) {
        fprintf(stderr, "ERROR: Cannot create output archive: %s\n", archive_path);
        free_tree(root);
        free(buffer);
        fclose(in);
        return 0;
    }

    // Write Header: Magic identifier
    fwrite("HUF2", 1, 4, out);

    // Store original filename for auto-restoration
    const char* base_name = get_filename_only(input_path);
    uint16_t name_len = (uint16_t)strlen(base_name);
    fwrite(&name_len, sizeof(uint16_t), 1, out);
    fwrite(base_name, 1, name_len, out);

    // Metadata
    fwrite(&total_bytes, sizeof(uint64_t), 1, out);
    fwrite(&unique_symbols, sizeof(uint16_t), 1, out);

    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] > 0) {
            uint8_t sym = (uint8_t)i;
            fwrite(&sym, sizeof(uint8_t), 1, out);
            fwrite(&frequencies[i], sizeof(uint64_t), 1, out);
        }
    }

    fwrite(&total_encoded_bits, sizeof(uint64_t), 1, out);

    // Pass 2: Stream bit encoding
    rewind(in);
    BitWriter bw;
    bit_writer_init(&bw, out);

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, in)) > 0) {
        for (size_t i = 0; i < bytes_read; ++i) {
            bit_writer_write(&bw, codes[buffer[i]]);
        }
    }
    bit_writer_flush(&bw);

    free_tree(root);
    free(buffer);
    fclose(in);
    fclose(out);
    return 1;
}

// ============================================================
// Decompression Implementation
// ============================================================
int extract_file(const char* archive_path, const char* custom_output_path) {
    FILE* in = fopen(archive_path, "rb");
    if (!in) {
        fprintf(stderr, "ERROR: Cannot open archive: %s\n", archive_path);
        return 0;
    }

    char magic[4];
    if (fread(magic, 1, 4, in) != 4 || memcmp(magic, "HUF2", 4) != 0) {
        fprintf(stderr, "ERROR: Invalid or incompatible .huff archive.\n");
        fclose(in);
        return 0;
    }

    uint16_t name_len = 0;
    fread(&name_len, sizeof(uint16_t), 1, in);
    char original_name[512] = {0};
    fread(original_name, 1, name_len, in);

    char output_target[1024];
    if (custom_output_path && strlen(custom_output_path) > 0) {
        strcpy(output_target, custom_output_path);
    } else {
        snprintf(output_target, sizeof(output_target), "extracted_%s", original_name);
    }

    uint64_t original_size = 0;
    uint16_t unique_symbols = 0;
    fread(&original_size, sizeof(uint64_t), 1, in);
    fread(&unique_symbols, sizeof(uint16_t), 1, in);

    uint64_t frequencies[256] = {0};
    for (uint16_t i = 0; i < unique_symbols; ++i) {
        uint8_t sym;
        uint64_t freq;
        fread(&sym, sizeof(uint8_t), 1, in);
        fread(&freq, sizeof(uint64_t), 1, in);
        frequencies[sym] = freq;
    }

    uint64_t total_encoded_bits = 0;
    fread(&total_encoded_bits, sizeof(uint64_t), 1, in);

    HuffmanNode* root = build_huffman_tree(frequencies);

    FILE* out = fopen(output_target, "wb");
    if (!out) {
        fprintf(stderr, "ERROR: Cannot create output file: %s\n", output_target);
        free_tree(root);
        fclose(in);
        return 0;
    }

    uint8_t* write_buffer = (uint8_t*)malloc(BUFFER_SIZE);
    size_t buffer_idx = 0;
    uint64_t bytes_decoded = 0;

    // Single-symbol edge case
    if (root && root->left == NULL && root->right == NULL) {
        memset(write_buffer, root->data, BUFFER_SIZE);
        while (bytes_decoded < original_size) {
            size_t chunk = (original_size - bytes_decoded > BUFFER_SIZE)
                               ? BUFFER_SIZE
                               : (size_t)(original_size - bytes_decoded);
            fwrite(write_buffer, 1, chunk, out);
            bytes_decoded += chunk;
        }
    } else {
        BitReader br;
        bit_reader_init(&br, in);
        HuffmanNode* curr = root;

        while (bytes_decoded < original_size) {
            int bit = bit_reader_read_bit(&br);
            if (bit == -1) break;

            curr = (bit == 0) ? curr->left : curr->right;
            if (curr->left == NULL && curr->right == NULL) {
                write_buffer[buffer_idx++] = curr->data;
                bytes_decoded++;

                if (buffer_idx == BUFFER_SIZE) {
                    fwrite(write_buffer, 1, BUFFER_SIZE, out);
                    buffer_idx = 0;
                }
                curr = root;
            }
        }

        if (buffer_idx > 0) {
            fwrite(write_buffer, 1, buffer_idx, out);
        }
    }

    free(write_buffer);
    free_tree(root);
    fclose(in);
    fclose(out);

    return (bytes_decoded == original_size);
}

// ============================================================
// CLI & Interactive Menu
// ============================================================
int main(int argc, char* argv[]) {
    // Command line mode (invoked by GUI)
    if (argc >= 3) {
        if (strcmp(argv[1], "-c") == 0 && argc >= 4) {
            return compress_file(argv[2], argv[3]) ? 0 : 1;
        } else if (strcmp(argv[1], "-x") == 0) {
            const char* target = (argc >= 4) ? argv[3] : "";
            return extract_file(argv[2], target) ? 0 : 1;
        }
    }

    // Interactive Terminal Mode
    printf("====================================================\n");
    printf("     HUFFMAN BINARY ARCHIVER & EXTRACTOR (C-CORE)   \n");
    printf("====================================================\n");
    printf("1. Compress File (PDF, Image, Video, Text)\n");
    printf("2. Extract Archive (.huff)\n");
    printf("Enter choice: ");

    int choice;
    if (scanf("%d", &choice) != 1) return 0;
    while (getchar() != '\n'); // Clear input buffer

    char in_path[512], out_path[512];

    if (choice == 1) {
        printf("Enter input file path: ");
        if (!fgets(in_path, sizeof(in_path), stdin)) return 0;
        in_path[strcspn(in_path, "\r\n")] = 0;

        printf("Enter archive destination (or Enter for archive.huff): ");
        if (!fgets(out_path, sizeof(out_path), stdin)) return 0;
        out_path[strcspn(out_path, "\r\n")] = 0;
        if (strlen(out_path) == 0) strcpy(out_path, "archive.huff");

        if (compress_file(in_path, out_path)) {
            printf("\nSUCCESS: Compressed '%s' into '%s'\n", in_path, out_path);
        }
    } else if (choice == 2) {
        printf("Enter .huff archive path: ");
        if (!fgets(in_path, sizeof(in_path), stdin)) return 0;
        in_path[strcspn(in_path, "\r\n")] = 0;

        printf("Enter output filename (or Enter for auto-restore): ");
        if (!fgets(out_path, sizeof(out_path), stdin)) return 0;
        out_path[strcspn(out_path, "\r\n")] = 0;

        if (extract_file(in_path, out_path)) {
            printf("\nSUCCESS: Archive extracted successfully.\n");
        }
    }
    return 0;
}