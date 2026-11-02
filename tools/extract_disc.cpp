// Unpacks every file from a PS2 CD/DVD image into a folder (same result as tools/iso9660.py).
//
//   extract_disc <image.bin|image.iso> <output-dir>
//
// Accepts raw 2352-byte-sector images (Mode 2 or Mode 1, e.g. a .bin from a .cue/.bin pair)
// and plain 2048-byte-sector .iso images. Build: g++ -O2 -std=c++17 extract_disc.cpp
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    std::ifstream g_img;
    uint32_t g_sectorSize = 2048;
    uint32_t g_dataOffset = 0;

    bool readSector(uint32_t lba, uint8_t *out2048)
    {
        g_img.clear();
        g_img.seekg(static_cast<std::streamoff>(lba) * g_sectorSize + g_dataOffset);
        g_img.read(reinterpret_cast<char *>(out2048), 2048);
        return g_img.gcount() == 2048;
    }

    std::vector<uint8_t> readExtent(uint32_t lba, uint32_t size)
    {
        std::vector<uint8_t> out((static_cast<size_t>(size) + 2047) / 2048 * 2048);
        for (size_t i = 0; i * 2048 < out.size(); ++i)
        {
            if (!readSector(lba + static_cast<uint32_t>(i), out.data() + i * 2048))
                throw std::runtime_error("image is truncated");
        }
        out.resize(size);
        return out;
    }

    uint32_t le32(const uint8_t *p)
    {
        return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }

    struct Entry
    {
        std::string path;
        uint32_t lba;
        uint32_t size;
    };

    void walk(uint32_t lba, uint32_t size, const std::string &path, std::vector<Entry> &files, int depth)
    {
        if (depth > 16)
            return;
        const std::vector<uint8_t> dir = readExtent(lba, size);
        size_t i = 0;
        while (i < dir.size())
        {
            const uint8_t len = dir[i];
            if (len == 0)
            {
                i = (i / 2048 + 1) * 2048;
                continue;
            }
            if (i + len > dir.size() || len < 34)
                break;
            const uint8_t *rec = dir.data() + i;
            const uint32_t elba = le32(rec + 2);
            const uint32_t esize = le32(rec + 10);
            const uint8_t flags = rec[25];
            const uint8_t nameLen = rec[32];
            std::string name(reinterpret_cast<const char *>(rec + 33), nameLen);
            i += len;
            if (nameLen == 1 && (name[0] == 0 || name[0] == 1))
                continue;
            if (const size_t semi = name.find(';'); semi != std::string::npos)
                name.resize(semi);
            const std::string full = path.empty() ? name : path + "/" + name;
            if (flags & 2)
                walk(elba, esize, full, files, depth + 1);
            else
                files.push_back({full, elba, esize});
        }
    }

    bool detectLayout(uint64_t fileSize)
    {
        struct Layout { uint32_t sectorSize, dataOffset; };
        const Layout layouts[] = {{2352, 24}, {2352, 16}, {2048, 0}};
        for (const Layout &l : layouts)
        {
            g_sectorSize = l.sectorSize;
            g_dataOffset = l.dataOffset;
            if (fileSize < 17ull * l.sectorSize)
                continue;
            uint8_t pvd[2048];
            if (readSector(16, pvd) && pvd[0] == 1 && std::memcmp(pvd + 1, "CD001", 5) == 0)
                return true;
        }
        return false;
    }
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: extract_disc <image.bin|image.iso> <output-dir>\n";
        return 2;
    }
    try
    {
        const fs::path image = fs::u8path(argv[1]);
        const fs::path outDir = fs::u8path(argv[2]);
        g_img.open(image, std::ios::binary);
        if (!g_img)
        {
            std::cerr << "Can't open " << argv[1] << "\n";
            return 1;
        }
        if (!detectLayout(fs::file_size(image)))
        {
            std::cerr << "Not a CD/DVD image (no ISO9660 volume found): " << argv[1] << "\n";
            return 1;
        }
        uint8_t pvd[2048];
        readSector(16, pvd);
        std::string volume(reinterpret_cast<char *>(pvd + 40), 32);
        volume.erase(volume.find_last_not_of(' ') + 1);
        std::cout << "Volume: " << volume << " (" << g_sectorSize << "-byte sectors)\n";

        std::vector<Entry> files;
        walk(le32(pvd + 156 + 2), le32(pvd + 156 + 10), "", files, 0);

        uint64_t total = 0;
        for (const Entry &e : files)
        {
            const fs::path dst = outDir / fs::u8path(e.path);
            fs::create_directories(dst.parent_path());
            std::ofstream out(dst, std::ios::binary);
            std::vector<uint8_t> chunk(2048 * 512);
            uint32_t remaining = e.size;
            uint32_t lba = e.lba;
            while (remaining > 0)
            {
                const uint32_t sectors = std::min<uint32_t>(512u, (remaining + 2047) / 2048);
                for (uint32_t s = 0; s < sectors; ++s)
                {
                    if (!readSector(lba + s, chunk.data() + s * 2048))
                        throw std::runtime_error("image is truncated");
                }
                const uint32_t bytes = std::min<uint32_t>(remaining, sectors * 2048);
                out.write(reinterpret_cast<const char *>(chunk.data()), bytes);
                remaining -= bytes;
                lba += sectors;
            }
            if (!out)
                throw std::runtime_error("can't write " + dst.string());
            total += e.size;
            std::cout << "  " << e.path << " (" << e.size << " bytes)\n";
        }
        std::cout << files.size() << " files, " << total << " bytes extracted to " << outDir.string() << "\n";
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
