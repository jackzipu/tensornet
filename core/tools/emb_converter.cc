// Copyright (c) 2020, Qihoo, Inc.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <queue>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>

#include "core/utility/file_io.h"
#include "core/utility/mpi_manager.h"

#include "tensorflow/core/platform/env.h"

#define CHECK_TF_STATUS(status)                             \
    do {                                                    \
        auto s = (status);                                  \
        CHECK(s.ok()) << s;                                 \
    } while (0)

void GetAllFiles(const std::string& path, std::vector<std::string>& files) {
    CHECK_TF_STATUS(tensorflow::Env::Default()->FileExists(path));

    std::queue<std::string> cur;
    cur.emplace(path);

    while (!cur.empty()) {
        std::vector<std::string> childs;
        CHECK_TF_STATUS(tensorflow::Env::Default()->GetChildren(cur.front(), &childs));
        for (auto& item : childs) {
            std::string child_path = cur.front() + "/" + item;
            tensorflow::Status status = tensorflow::Env::Default()->IsDirectory(child_path);
            if (status.ok()) {
                cur.emplace(child_path);
            } else {
                files.emplace_back(child_path);
            }
        }
        cur.pop();
    }
    std::sort(files.begin(), files.end());
}

int ParseAdaGradParams(const std::string& file, butil::IOBuf& buf) {
    butil::IOBuf buf_in;
    if (tensornet::read_from_file(file, buf_in) < 0) {
        LOG(ERROR) << "read_from_file [" << file << "] failed.";
        return -1;
    }

    int dim = 0;

    if (buf_in.size() == 0) {
        LOG(INFO) << "file [" << file << "] processed.";
        return 0;
    }

    CHECK_EQ(sizeof(int), buf_in.cutn(&dim, sizeof(dim)));

    float weight[dim];

    std::vector<std::string> vec;
    boost::split(vec, file, boost::is_any_of("/"));
    std::string table_handle = vec[vec.size() - 3];

    while (buf_in.size()) {
        CHECK(buf_in.size() > sizeof(uint64_t));
        uint64_t key;
        CHECK_EQ(sizeof(uint64_t), buf_in.cutn(&key, sizeof(key)));
        buf.append(std::to_string(key) + "\t" + table_handle);

        size_t no_use_data_len = sizeof(float) + sizeof(uint32_t);
        CHECK(buf_in.size() >= dim * sizeof(float) + no_use_data_len + sizeof(float));

        CHECK_EQ(sizeof(float) * dim, buf_in.cutn(weight, sizeof(float) * dim));
        CHECK_EQ(no_use_data_len, buf_in.pop_front(no_use_data_len));

        float show;
        CHECK_EQ(sizeof(int), buf_in.cutn(&show, sizeof(float)));

        for (int i = 0; i < dim; ++i) {
            buf.append("\t" + std::to_string(weight[i]));
        }
        buf.append("\t" + std::to_string(show));

        buf.push_back('\n');
    }
    LOG(INFO) << "file [" << file << "] processed.";

    return 0;
}

int ParseAdamParams(const std::string& file, butil::IOBuf& buf) {
    // TODO (yaolei)

    return 0;
}

int Convert(const std::vector<std::string>& files, const std::string& out_file, const std::string& parse_mode, int rank) {
    butil::IOBuf buf;

    for (auto& file : files) {
        std::string flag = "/rank_" + std::to_string(rank) + "/";
        if (file.find(flag) == std::string::npos) {
            continue;
        }
        if (parse_mode.compare("AdaGrad") == 0) {
            if (ParseAdaGradParams(file, buf) < 0) {
                LOG(ERROR) << "ParseAdaGradParams [" << file << "] Failed.";
                return -1;
            }
        } else if (parse_mode.compare("Adam") == 0) {
            if (ParseAdamParams(file, buf) < 0) {
                LOG(ERROR) << "ParseAdaGradParams [" << file << "] Failed.";
                return -1;
            }
        }
    }

    if (tensornet::write_to_file(out_file, buf) < 0) {
        LOG(ERROR) << "Write data Failed.";
        return -1;
    } 

    return 0;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        LOG(ERROR) << "Wrong Command.\nUsage: " << argv[0] << " [input_path] [output_file] [AdaGrad or Adam]";
        return -1;
    }

    if (tensornet::MpiManager::Instance()->Init() != 0) {
        LOG(ERROR) << "MpiManager Init error.";
        return -1;
    }
    std::string input_path = argv[1];
    std::string out_file = argv[2];
    std::string parse_mode = argv[3];

    std::vector<std::string> files;

    GetAllFiles(input_path, files);

    int rank = tensornet::MpiManager::Instance()->Rank();
    out_file += "/part-" + std::to_string(rank);
    if (Convert(files, out_file, parse_mode, rank) < 0) {
        LOG(ERROR) << "Convert Failed.";
        return -1;
    }
    tensornet::MpiManager::Instance()->Barrier();

    return 0;
}
