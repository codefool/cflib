//
// Verify that all records in a DiskHashTable are unique, even accross buckets.
//
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "../include/libcf.h"

typedef uint64_t PositionId;

enum EndGameReason {
	EGR_NONE               = 0x00,
	//
	EGR_13A_CHECKMATE      = 0x01,
	EGR_14A_STALEMATE      = 0x02,
	EGR_14C_TRIPLE_OF_POS  = 0x03,
	EGR_14D1_KVK           = 0x04,
	EGR_14D2_KVKWBN        = 0x05,
	EGR_14D3_KWBVKWB       = 0x06,
	EGR_14D4_NO_LGL_MV_CM  = 0x07,
    EGR_14E1_LONE_KING     = 0x08,
    EGR_14E2_KBOKK         = 0x09,
    EGR_14E3_KNN           = 0x0a,
	EGR_14F_50_MOVE_RULE   = 0x0b
};

#pragma pack(1)
union MovePacked {
	uint16_t i;
	struct {
		uint16_t action    : 4;
		uint16_t source    : 6;
		uint16_t target    : 6;
	} f;
};

union GameInfoPacked {
	uint32_t i;
	struct {
		uint32_t unused            : 15; // for future use
		// en passant
		// If set, the pawn that rests on file en_passant_file moved two
		// positions. This signals that a pawn subject to en passant capture
		// exists, and does not mean that there is an opposing pawn that can
		// affect it.
		uint32_t en_passant_file   :  4; // file number where pawn rests
		// Castling is possible only if the participating pieces have not
		// moved (among other rules, but have nothing to do with prior movement.)
        uint32_t castle_rights     :  4;
		//
		// number of active pieces on the board (2..32)
		uint32_t on_move           :  1; // 0=white on-move, 1=black
		uint32_t piece_cnt         :  8;
	} f;
};

struct PositionPacked
{
    GameInfoPacked gi;
    uint64_t       pop;   // population bitmap
    uint64_t       lo;    // lo 64-bits population info
    uint64_t       hi;    // hi 64-bits population info
    bool operator<(const PositionPacked& o) const
    {
        if ( pop == o.pop )
        {
            if ( gi.i == o.gi.i )
            {
                if ( hi == o.hi )
                {
                    return lo < o.lo;
                }
                return hi < o.hi;
            }
            return gi.i < o.gi.i;
        }
        return pop < o.pop;
    }
};
#pragma pack()

struct PosInfo
{
    PositionId     id;        // unique id for this position
    PositionId     parent;    // the parent of this position
    MovePacked     move;      // the Move that created in this position
    short          move_cnt;  // number of valid moves for this position
    short          distance;  // number of moves from the initial position
    EndGameReason  egr;       // end game reason
    bool operator==(const PosInfo& other)
    {
        if (id       != other.id
        || parent    != other.parent
        || move.i    != other.move.i
        || move_cnt  != other.move_cnt
        || distance  != other.distance
        || egr       != other.egr
        )
            return false;
        return true;
    }  
};

void usage(std::string prog)
{
    std::cout << "DiskHashTable Utility\n"
              << "usage:\n"
              << '\t' << prog << " verify <path_to_dht_root> <dht_base_name> [options]\n"
              << '\t' << prog << " test [options]\n"
              << "note: there are no options yet\n"
              << std::endl;
    exit(1);
}

void command_verify(int argc, char **argv)
{
    if (argc < 4)
        usage(argv[0]);

    std::set<PositionPacked> cache;
    int rec_cnt(0);
    int dupe_cnt(0);

    std::filesystem::path path(argv[2]);
    if (!std::filesystem::exists(path))
    {
        std::cerr << path << " does not exist" << std::endl;
        exit(1);
    }
    std::string base(argv[3]);
    std::string min_buck;
    std::string max_buck;
    int frec_min(999999999);
    int frec_max(-1);
    int buckets = 1 << (4 * BUCKET_ID_WIDTH );
    for (int i = 0; i < buckets; i++)
    {
        char bucket[BUCKET_ID_WIDTH + 1];
        std::sprintf(bucket, "%0*x", BUCKET_ID_WIDTH, i);

        std::string fspec = libcf::DiskHashTable::get_bucket_fspec(path, base, bucket);
        if (!std::filesystem::exists(fspec))
        {
            std::cerr << fspec << " does not exist" << std::endl;
            continue;
        }
        std::FILE *fp = std::fopen( fspec.c_str(), "r" );
        if ( fp == nullptr )
        {
            std::cerr << "Error opening bucket file " << fspec << ' ' << errno << " - terminating" << std::endl;
            exit(errno);
        }
        PositionPacked pp;
        PosInfo pi;
        int frec_cnt(0);
        while (std::fread(&pp, sizeof(PositionPacked), 1, fp) == 1)
        {
            std::fread(&pi, sizeof(PosInfo), 1, fp);
            frec_cnt++;
            if (cache.contains(pp))
                dupe_cnt++;
            else
                cache.insert(pp);
            if ((++rec_cnt % 1000) == 0 )
                std::cout << fspec << ':' << cache.size() << ' ' << dupe_cnt << ' ' << frec_cnt << '\r' << std::flush;
        }
        std::fclose(fp);
        if ( frec_cnt < frec_min)
        {
            frec_min = frec_cnt;
            min_buck = bucket;
        }
        if ( frec_cnt > frec_max)
        {
            frec_max = frec_cnt;
            max_buck = bucket;
        }
    }
    std::cout << '\n'
              << cache.size() << ' '
              << min_buck << ':' << frec_min << ' '
              << max_buck << ':' << frec_max << ' '
              << (frec_max - frec_min)
              << std::endl;
}

void command_test(int argc, char **argv)
{
    // create a temporary dht
    // fill it with 10^4 key/values
    // read them back and verify
    // update all even-numbered keys
    // verify the changes
    // close and delete the dht
    PositionPacked i_pp, o_pp;
    PosInfo i_pi, o_pi;
    libcf::DiskHashTable dht;
    std::memset(&i_pp, 0x00, sizeof(PositionPacked));
    std::memset(&i_pi, 0x00, sizeof(PosInfo));
    dht.open("/tmp/", "temp", 888, sizeof(PositionPacked), sizeof(PosInfo));
    for (int i = 1; i < 100000; ++i)
    {
        i_pp.lo = i_pi.id = i;
        dht.append((libcf::ucharptr_c)&i_pp, (libcf::ucharptr_c)&i_pi);
        bool found = dht.search((libcf::ucharptr_c)&i_pp, (libcf::ucharptr)&o_pi);
        assert(found == true);
        assert(o_pi == i_pi);
        // update
        i_pi.distance = 2 * i;
        dht.update((libcf::ucharptr_c)&i_pp, (libcf::ucharptr_c)&i_pi);
        found = dht.search((libcf::ucharptr_c)&i_pp, (libcf::ucharptr)&o_pi);
        assert(found == true);
        assert(o_pi == i_pi);
        std::cout << i << std::endl;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
        usage(argv[0]);

    std::string cmd = argv[1];
    if ( cmd == "verify" )
        command_verify(argc, argv);
    else if (cmd == "test" )
        command_test(argc, argv);
    else
    {
        std::cerr << "Unknown command '" << cmd << '\'' << std::endl;
        exit(1);
    }
   return 0;
}