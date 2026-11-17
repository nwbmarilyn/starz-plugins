#pragma once
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace userpresets {

namespace fs = std::filesystem;

inline fs::path baseDir()
{
    const char* h = nullptr;
#if defined(_WIN32)
    h = std::getenv("APPDATA");
    if (h && *h) return fs::path(h) / "Starz" / "presets";
    h = std::getenv("USERPROFILE");
    return fs::path(h ? h : ".") / "Starz" / "presets";
#elif defined(__APPLE__)
    h = std::getenv("HOME");
    return fs::path(h ? h : ".") / "Library" / "Application Support" / "Starz" / "presets";
#else
    h = std::getenv("HOME");
    return fs::path(h ? h : ".") / ".config" / "Starz" / "presets";
#endif
}

inline fs::path dirFor(const std::wstring& key)
{
    return baseDir() / fs::path(key);
}

inline bool ensureDir(const fs::path& d)
{
    std::error_code ec;
    fs::create_directories(d, ec);
    return !ec;
}

inline std::vector<fs::path> list(const std::wstring& key)
{
    std::vector<fs::path> out;
    std::error_code ec;
    fs::path d = dirFor(key);
    if (!fs::exists(d, ec)) return out;
    for (auto& e : fs::directory_iterator(d, ec)) {
        if (e.is_regular_file(ec) && e.path().extension() == ".starz") out.push_back(e.path());
    }
    std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) { return a.filename() < b.filename(); });
    return out;
}

inline bool save(const fs::path& file, const std::vector<double>& v)
{
    if (!ensureDir(file.parent_path())) return false;
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s.precision(12);
    s << "STARZ-PRESET 1\n" << v.size() << "\n";
    for (double x : v) s << x << "\n";
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    std::string t = s.str();
    f.write(t.data(), (std::streamsize)t.size());
    return (bool)f;
}

inline bool load(const fs::path& file, std::vector<double>& v)
{
    std::ifstream f(file, std::ios::binary);
    if (!f) return false;
    std::string head;
    std::getline(f, head);
    if (head.rfind("STARZ-PRESET", 0) != 0) return false;
    f.imbue(std::locale::classic());
    size_t n = 0;
    f >> n;
    if (n == 0 || n > 256) return false;
    v.assign(n, 0.0);
    for (size_t i = 0; i < n; i++) {
        if (!(f >> v[i])) return false;
        v[i] = std::min(1.0, std::max(0.0, v[i]));
    }
    return true;
}

}
