#include <bitset>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <streambuf>
#include <string>

#include "json.hpp"

namespace huffman {
using nlohmann::json;

struct Node {
  int w = 1;
  char ch = '\0';
  Node* lhs = nullptr;
  Node* rhs = nullptr;
};

std::map<char, int> genStats(const std::string& str) {
  std::map<char, int> mp;
  for (const auto& ch : str) ++mp[ch];
  return mp;
}

Node* genTree(const std::map<char, int>& mp) {
  auto cmp = [](Node* a, Node* b) { return a->w > b->w; };
  std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> q;

  for (auto [ch, cnt] : mp) q.push(new Node{cnt, ch, nullptr, nullptr});

  while (q.size()) {
    if (q.size() == 1) return q.top();

    auto a = q.top();
    q.pop();
    auto b = q.top();
    q.pop();
    q.push(new Node{a->w + b->w, '\0', a, b});
  }
  return new Node{};
}

json tree2json(Node* nd) {
  return (nd->ch ? json({{"char", std::string(1, nd->ch)}})
                 : json({
                       {"0", tree2json(nd->lhs)},
                       {"1", tree2json(nd->rhs)},
                   }));
}

void genTable(std::map<char, std::string>& mp, Node* nd, std::string par) {
  if (nd->ch != '\0') {
    mp[nd->ch] = par;
  } else {
    genTable(mp, nd->lhs, par + "0");
    genTable(mp, nd->rhs, par + "1");
  }
}

std::map<char, std::string> genTable(Node* nd) {
  std::map<char, std::string> mp;
  genTable(mp, nd, "");
  return mp;
}

json table2json(const std::map<char, std::string>& table) {
  json j;
  for (auto [ch, str] : table) j[std::string(1, ch)] = str;
  return j;
}

std::string encode(json table, const std::string& str) {
  std::string res;
  for (const auto& ch : str)
    res += table[std::string(1, ch)].get<std::string>();
  return res;
}

std::string decode(json tree, const std::string& str) {
  std::string res;

  auto cur = tree;
  for (const auto& ch : str) {
    cur = cur[std::string(1, ch)];

    if (!cur["char"].is_null()) {
      res += cur["char"].get<std::string>();
      cur = tree;
    }
  }
  return res;
}

void writeBin(std::filesystem::path path, std::string str) {
  auto left = (8 - str.size() % 8) % 8;
  if (left) str += std::string(left, '0');

  std::vector<std::byte> data;
  data.push_back(static_cast<std::byte>(left));
  for (int i = 0; i < str.size() / 8; ++i)
    data.push_back(
        static_cast<std::byte>(std::stoi(str.substr(i * 8, 8), nullptr, 2)));

  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

std::string readBin(std::filesystem::path path) {
  auto f = std::ifstream(path);

  std::stringstream buffer;
  buffer << f.rdbuf();
  f.close();
  const auto data = buffer.str();

  std::string res;
  res.reserve((data.size() - 1) * 8);

  for (int i = 1; i < data.size(); ++i)
    res += std::bitset<8>(static_cast<int>(data[i])).to_string();

  for (int i = 0; i < static_cast<int>(data[0]); ++i) res.pop_back();

  return res;
}

std::string read(const std::filesystem::path& text_in,
                 const std::filesystem::path& tree_in) {
  json j_tree;
  std::ifstream(tree_in) >> j_tree;
  return decode(j_tree, readBin(text_in));
}

void write(const std::filesystem::path& text_in,
           const std::filesystem::path& text_out,
           const std::filesystem::path& tree_out) {
  auto text_file = std::ifstream(text_in);
  std::stringstream buffer;
  buffer << text_file.rdbuf();
  text_file.close();
  auto str = buffer.str();

  auto tree = genTree(genStats(str));

  writeBin(text_out, encode(table2json(genTable(tree)), str));

  auto tree_file = std::ofstream(tree_out);
  tree_file << tree2json(tree).dump(2);
}
}  // namespace huffman

int main() {
  huffman::write("text.txt", "out.bin", "tree.json");
  std::cout << huffman::read("out.bin", "tree.json") << std::endl;
}
