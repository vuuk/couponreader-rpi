#ifndef READINGSMODEL_H
#define READINGSMODEL_H

#include <QAbstractTableModel>
#include <memory>
#include <QVector>
#include <QDebug>

#include "sensor_reading.h"
#include "serialcom_handler.h"


extern template class Serialcom_handler<sensor_reading>;
class ReadingsModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ReadingsModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation = Qt::Horizontal,
                        int role = Qt::DisplayPropertyRole) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVector<sensor_reading>* get_results() { return results; }
signals:

public slots:
    void read_from_spsc();
private:
   Serialcom_handler<sensor_reading> &handler;
   QVector<sensor_reading> *results;
};

#endif // READINGSMODEL_H
