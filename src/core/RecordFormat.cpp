#include "RecordFormat.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

// Text with spaces is wrapped in quotes. Text without spaces is written as is.
// Empty text is written as "-".
std::string quoteField(const std::string& text)
{
    if (text.empty())
        return "-";

    bool needsQuotes = false;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '"' || c == '\\') {
            needsQuotes = true;
            break;
        }
    }

    if (!needsQuotes)
        return text;

    std::string out = "\"";
    for (char c : text) {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    out += '"';
    return out;
}
std::string writeRecord(const std::vector<std::string>& fields)
{
    std::string out;
    for (const auto& f : fields) {
        out += f;
        out += ' ';
    }
    out += '#';
    return out;
}

OperationResult splitRecord(
    const std::string& line,
    int lineNumber,
    std::vector<std::string>& fields)
{
    OperationResult r;
    fields.clear();

    const std::size_t n = line.size();
    std::size_t i = 0;
    bool sawEnd = false;

    while (i < n) {
        while (i < n && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= n) break;

        if (line[i] == '"') {
            std::string cur;
            ++i;
            bool closed = false;
            while (i < n) {
                if (line[i] == '\\' && i + 1 < n) {
                    cur += line[i + 1];
                    i += 2;
                } else if (line[i] == '"') {
                    closed = true;
                    ++i;
                    break;
                } else {
                    cur += line[i++];
                }
            }
            if (!closed) {
                r.message = "line " + std::to_string(lineNumber)
                          + ": unterminated quote";
                return r;
            }
            fields.push_back(cur);
        } else {
            std::string cur;
            while (i < n && line[i] != ' ' && line[i] != '\t')
                cur += line[i++];
            if (cur == "#") { sawEnd = true; break; }
            if (cur == "-") cur.clear();   // "-" means empty field
            fields.push_back(cur);
        }
    }

    if (!sawEnd) {
        r.message = "line " + std::to_string(lineNumber)
                  + ": missing final #";
        return r;
    }

    r.success = true;
    return r;
}

OperationResult readLines(
    const std::string& path,
    std::vector<std::string>& lines)
{
    OperationResult r;
    lines.clear();

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        r.message = "Cannot open " + path;
        return r;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.find_first_not_of(" \t") == std::string::npos)
            continue;   // skip blank lines

        lines.push_back(line);
    }

    r.success = true;
    return r;
}

void FileTransaction::add(
    const std::string& targetPath,
    const std::string& newContent)
{
    entries_.push_back({ targetPath, newContent });
}

OperationResult FileTransaction::commit()
{
    OperationResult r;

    // Step 1: write every file to a temp name first.
    bool ok = !simulateFailure_;

    for (std::size_t k = 0; ok && k < entries_.size(); ++k) {
        std::ofstream out(entries_[k].target + ".tmp",
                          std::ios::binary | std::ios::trunc);
        out << entries_[k].content;
        out.close();
        if (!out) ok = false;
    }

    if (!ok) {
        for (const auto& e : entries_)
            std::remove((e.target + ".tmp").c_str());
        r.message = "Write failed. No file was changed.";
        return r;
    }

    // Step 2: all temp files are ready, now replace the real files.
    for (const auto& e : entries_) {
        std::remove(e.target.c_str());
        if (std::rename((e.target + ".tmp").c_str(), e.target.c_str()) != 0) {
            r.message = "Could not replace " + e.target;
            return r;
        }
    }

    entries_.clear();
    r.success = true;
    r.message = "Saved.";
    return r;
}