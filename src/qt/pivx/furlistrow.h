#ifndef FURLISTROW_H
#define FURLISTROW_H

#include <QWidget>
#include <QColor>

template <class T=QWidget*>
class FurListRow{

public:
    FurListRow(){}
    virtual inline ~FurListRow(){}

    virtual inline T createHolder(int){
       return nullptr;
    }

    virtual inline void init(T, bool isHovered, bool isSelected) const{

    }

    virtual inline QColor rectColor(bool isHovered, bool isSelected){
        return QColor();
    }
};

#endif // FURLISTROW_H
