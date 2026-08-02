#ifndef PIC_DESCRIPTOR_H
#define PIC_DESCRIPTOR_H

#include <string>

struct PicDescriptor
{
    std::string raw;
    int displaySize = 0;
    int storageSize = 0;
    int decimalPlaces = 0;
    bool isNumeric = false;
    bool isAlpha = false;
    bool isEdited = false;
    bool isSigned = false;
    bool isJustified = false;
    bool justifyLeft = false;
};

PicDescriptor analyzePicture(const std::string &rawPic);

#endif
