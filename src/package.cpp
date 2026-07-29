#include "package.h"

#include "nlpxfile.h"
#include "nlpzfile.h"

package *open_package(const std::string &path) {
    if (nlpz_file::has_magic(path)) {
        nlpz_file *nlpz = new nlpz_file();
        if (nlpz->open(path)) return nlpz;

        delete nlpz;
        return nullptr;
    }

    nlpx_file *nlpx = new nlpx_file();
    if (nlpx->open(path)) return nlpx;

    delete nlpx;
    return nullptr;
}

