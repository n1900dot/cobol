#include "pic_descriptor.h"
#include <algorithm>
#include <cctype>
#include <string>

PicDescriptor analyzePicture(const std::string &rawPic)
{
    PicDescriptor desc;
    desc.raw = rawPic;
    std::string pic = rawPic;
    pic.erase(std::remove_if(pic.begin(), pic.end(), ::isspace), pic.end());

    size_t i = 0;
    while (i < pic.size()) {
        char c = pic[i];
        int count = 1;

        if (i + 1 < pic.size() && pic[i + 1] == '(') {
            size_t j = i + 2;
            std::string numStr;
            while (j < pic.size() && std::isdigit(pic[j])) numStr += pic[j++];
            if (j < pic.size() && pic[j] == ')') {
                count = numStr.empty() ? 1 : std::stoi(numStr);
                i = j + 1;
            } else {
                i++;
            }
        } else {
            i++;
        }

        switch (c) {
        case '9':
            desc.isNumeric = true;
            desc.displaySize += count;
            desc.storageSize += count;
            break;
        case 'X': case 'A':
            desc.isAlpha = true;
            desc.displaySize += count;
            desc.storageSize += count;
            break;
        case 'S': desc.isSigned = true; break;
        case 'V': break;
        case 'P':
            desc.isNumeric = true;
            desc.displaySize += count;
            break;
        case 'Z': case '*':
            desc.isEdited = true;
            desc.isNumeric = true;
            desc.displaySize += count;
            desc.storageSize += count;
            break;
        case ',': case '.': case '$': case '+': case '-':
            desc.isEdited = true;
            desc.displaySize += count;
            desc.storageSize += count;
            break;
        case 'C':
            if (i < pic.size() && pic[i] == 'R') {
                desc.isEdited = true;
                desc.displaySize += 2;
                desc.storageSize += 2;
                i++;
            }
            break;
        case 'D':
            if (i < pic.size() && pic[i] == 'B') {
                desc.isEdited = true;
                desc.displaySize += 2;
                desc.storageSize += 2;
                i++;
            }
            break;
        default: break;
        }
    }

    size_t vPos = pic.find('V');
    if (vPos != std::string::npos) {
        size_t j = vPos + 1;
        while (j < pic.size()) {
            if (pic[j] == '9') {
                int count = 1;
                if (j + 1 < pic.size() && pic[j + 1] == '(') {
                    size_t k = j + 2;
                    std::string numStr;
                    while (k < pic.size() && std::isdigit(pic[k])) numStr += pic[k++];
                    if (k < pic.size() && pic[k] == ')') {
                        count = numStr.empty() ? 1 : std::stoi(numStr);
                        j = k + 1;
                    } else {
                        j++;
                    }
                } else {
                    j++;
                }
                desc.decimalPlaces += count;
            } else {
                j++;
            }
        }
    }

    if (desc.isSigned) desc.storageSize += 1;
    if (!desc.isNumeric && !desc.isAlpha) desc.isAlpha = true;

    std::string upperRaw;
    for (char c : rawPic) upperRaw += std::toupper(c);
    size_t justPos = upperRaw.find("JUST");
    if (justPos != std::string::npos) {
        desc.isJustified = true;
        size_t afterJust = justPos + 4;
        while (afterJust < upperRaw.size() && std::isspace(upperRaw[afterJust])) afterJust++;
        if (upperRaw.substr(afterJust, 4) == "LEFT") {
            desc.justifyLeft = true;
        } else if (afterJust + 5 <= upperRaw.size() && upperRaw.substr(afterJust, 5) == "RIGHT") {
            desc.justifyLeft = false;
        }
    }

    return desc;
}
