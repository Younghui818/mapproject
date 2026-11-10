#pragma once
#include <QDialog>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QVBoxLayout;
class QScrollArea;

// 路径元信息（UI 层展示用）
struct PathExportItem {
  quint64 id;
  QString name;
  double lengthMeters;
};

class MapPathExportDialog : public QDialog {
  Q_OBJECT
 public:
  explicit MapPathExportDialog(const QVector<PathExportItem>& paths,
                               QWidget* parent = nullptr);
  // 用户确认后调用
  QVector<quint64> selectedIds() const;
  QString format() const;  // "json"

  // 控件由 Qt 对象树管理，使用默认析构函数。
  ~MapPathExportDialog() = default;

 private slots:
  void SlotSelectAll();
  void SlotDeselectAll();

 private:
  static QString formatLength(double meters);

  QVector<PathExportItem> paths_;
  QVector<QCheckBox*> checkboxes_;
  QComboBox* combo_format_ = nullptr;
};
