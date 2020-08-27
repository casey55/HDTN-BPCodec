#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

#include "util/tsc.h"
#include "codec/bpv6.h"
#include "codec/bpv7.h"

#define BUNDLE_SZ_MAX  (8192)
#define BPV7_ENCODE_COUNT  (1 << 24)

using namespace hdtn;

int main(int argc, char* argv[]) {
    size_t sz = 0;
    uint64_t offset = 0;
    const char* payload = "Hello World!";

    uint8_t bpv6_buf[BUNDLE_SZ_MAX];
    uint8_t bpv7_buf[BUNDLE_SZ_MAX];

    memset(bpv6_buf, 0x42, BUNDLE_SZ_MAX);
    memset(bpv7_buf, 0x42, BUNDLE_SZ_MAX);

    int fd = open("../../test/bundle.bpbis-16.cbor", O_RDONLY);
    if(fd < 0) {
        perror("Failed to open target file:");
        return -1;
    }

    sz = read(fd, bpv7_buf, BUNDLE_SZ_MAX);
    if(sz < 0) {
        perror("Failed to read bundle data:");
        return -2;
    }

    uint64_t start = 0;
    uint64_t tsc_correction = 0;

    uint64_t tsc_bpv7_decode_elapsed = 0;
    uint64_t bpv7_decode_bytes = 0;
    uint64_t bpv7_decode_data_bytes = 0;

    uint64_t tsc_bpv6_decode_elapsed = 0;
    uint64_t bpv6_decode_bytes = 0;
    uint64_t bpv6_decode_data_bytes = 0;

    bpv6_primary_block bpv6_primary;
    bpv6_canonical_block bpv6_block;
    memset(&bpv6_primary, 0, sizeof(bpv6_primary_block));
    memset(&bpv6_block, 0, sizeof(bpv6_canonical_block));

    bpv6_primary.version = BPV6_CCSDS_VERSION;
    bpv6_primary.src_node = 1;
    bpv6_primary.src_svc = 1;
    bpv6_primary.dst_node = 2;
    bpv6_primary.dst_svc = 1;
    bpv6_primary.lifetime = 3600;
    bpv6_primary.creation = bpv6_unix_to_5050(time(0));
    bpv6_primary.sequence = 1;
    offset = bpv6_primary_block_encode(&bpv6_primary, (char *)bpv6_buf, offset, BUNDLE_SZ_MAX);


    // write a previous hop insertion header ...
    bpv6_block.type = BPV6_BLOCKTYPE_PREV_HOP_INSERTION;
    bpv6_block.flags = 0;
    bpv6_block.length = 2;
    offset += bpv6_canonical_block_encode(&bpv6_block, (char *)bpv6_buf, offset, BUNDLE_SZ_MAX);
    offset += 2;

    // write a trash extension block header ...
    bpv6_block.type = 0x50;
    bpv6_block.flags = 0;
    bpv6_block.length = 4;
    offset += bpv6_canonical_block_encode(&bpv6_block, (char *)bpv6_buf, offset, BUNDLE_SZ_MAX);
    offset += 4;

    // write a trash extension block header ...
    bpv6_block.type = 0x51;
    bpv6_block.flags = 0;
    bpv6_block.length = 6;
    offset += bpv6_canonical_block_encode(&bpv6_block, (char *)bpv6_buf, offset, BUNDLE_SZ_MAX);
    offset += 6;

    bpv6_block.type = BPV6_BLOCKTYPE_PAYLOAD;
    bpv6_block.flags = BPV6_BLOCKFLAG_LAST_BLOCK;
    bpv6_block.length = 11;
    offset += bpv6_canonical_block_encode(&bpv6_block, (char *)bpv6_buf, offset, BUNDLE_SZ_MAX);
    memcpy(bpv6_buf + offset, payload, strlen(payload));
    offset += strlen(payload);

    bpv7_primary_block bpv7_primary;
    bpv7_canonical_block bpv7_block;
    memset(&bpv7_primary, 0, sizeof(bpv7_primary_block));
    memset(&bpv7_block, 0, sizeof(bpv7_canonical_block));

    // Build a correction factor ...
    for(uint64_t i = 0; i < BPV7_ENCODE_COUNT; ++i) {
        start = rdtsc_time_start();
        tsc_correction += rdtsc_time_end() - start;
    }

    for(uint64_t i = 0; i < BPV7_ENCODE_COUNT; ++i) {
        memset(&bpv7_block, 0, sizeof(bpv7_canonical_block));
        start = rdtsc_time_start();
        offset = bpv7_primary_block_decode(&bpv7_primary, (char *)bpv7_buf, 0, BUNDLE_SZ_MAX);
        while(bpv7_block.block_type != BPV7_BLOCKTYPE_PAYLOAD) {
            offset += bpv7_canonical_block_decode(&bpv7_block, (char *)bpv7_buf, offset, BUNDLE_SZ_MAX);
        }
        tsc_bpv7_decode_elapsed += rdtsc_time_end() - start;
        bpv7_decode_bytes += offset;
        bpv7_decode_data_bytes += bpv7_block.len; // since last block is always the payload block ...
    }

    memset(&bpv6_primary, 0, sizeof(bpv6_primary_block));
    memset(&bpv6_block, 0, sizeof(bpv6_canonical_block));

    for(uint64_t i = 0; i < BPV7_ENCODE_COUNT; ++i) {
        memset(&bpv6_block, 0, sizeof(bpv6_canonical_block));
        start = rdtsc_time_start();
        offset = bpv6_primary_block_decode(&bpv6_primary, (char *)bpv6_buf, 0, BUNDLE_SZ_MAX);
        while((bpv6_block.flags & BPV6_BLOCKFLAG_LAST_BLOCK) != BPV6_BLOCKFLAG_LAST_BLOCK) {
            offset += bpv6_canonical_block_decode(&bpv6_block, (char *)bpv6_buf, offset, BUNDLE_SZ_MAX);
            offset += bpv6_block.length;
        }
        tsc_bpv6_decode_elapsed += rdtsc_time_end() - start;
        bpv6_decode_bytes += offset;
        bpv6_decode_data_bytes += strlen(payload);
    }

    printf("Execution completed.  Estimating tsc frequency ...\n");
    uint64_t freq = tsc_freq(5000000);
    freq /= 5;
    printf("Done.\n");

    std::cout << "[BPv6]" << std::endl;
    std::cout << "Decode rdtsc total: " << tsc_bpv6_decode_elapsed << std::endl;
    std::cout << "Corrected: " << tsc_bpv6_decode_elapsed - tsc_correction << std::endl;
    std::cout << "Total decoded bytes: " << bpv6_decode_bytes << std::endl;
    std::cout << "Total decoded payload bytes: " << bpv6_decode_data_bytes << std::endl;
    std::cout << "Headers completed: " << BPV7_ENCODE_COUNT << std::endl;

    printf("Raw average rdtsc per decode: %0.2f\n", tsc_bpv6_decode_elapsed / (double)BPV7_ENCODE_COUNT);
    printf("Corrected average rdtsc per decode: %0.2f\n", (tsc_bpv6_decode_elapsed - tsc_correction) / (double)BPV7_ENCODE_COUNT);
    printf("Raw headers per second: %0.2f\n", freq / (tsc_bpv6_decode_elapsed / (double)BPV7_ENCODE_COUNT));
    printf("Corrected headers per second: %0.2f\n", freq / ((tsc_bpv6_decode_elapsed - tsc_correction) / (double)BPV7_ENCODE_COUNT));
    
    std::cout << "[BPv7]" << std::endl;
    std::cout << "Decode rdtsc total: " << tsc_bpv7_decode_elapsed << std::endl;
    std::cout << "Corrected: " << tsc_bpv7_decode_elapsed - tsc_correction << std::endl;
    std::cout << "Total decoded bytes: " << bpv7_decode_bytes << std::endl;
    std::cout << "Total decoded payload bytes: " << bpv7_decode_data_bytes << std::endl;
    std::cout << "Headers completed: " << BPV7_ENCODE_COUNT << std::endl;

    printf("Raw average rdtsc per decode: %0.2f\n", tsc_bpv7_decode_elapsed / (double)BPV7_ENCODE_COUNT);
    printf("Corrected average rdtsc per decode: %0.2f\n", (tsc_bpv7_decode_elapsed - tsc_correction) / (double)BPV7_ENCODE_COUNT);
    printf("Raw headers per second: %0.2f\n", freq / (tsc_bpv7_decode_elapsed / (double)BPV7_ENCODE_COUNT));
    printf("Corrected headers per second: %0.2f\n", freq / ((tsc_bpv7_decode_elapsed - tsc_correction) / (double)BPV7_ENCODE_COUNT));

    return 0;
}
