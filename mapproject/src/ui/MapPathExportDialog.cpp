#include "MapPathExportDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
QString FormatLength(double meters) {
  if (meters < 1.0) return QString::number(meters * 100.0, 'f', 1) + " cm";
  if (meters < 1000.0) return QString::number(meters, 'f', 1) + " m";
  return QString::number(meters / 1000.0, 'f', 2) + " km";
}
}  // namespace

MapPathExportDialog::MapPathExportDialog(const QVector<PathExportItem>& paths,
                                         QWidget* parent)
    : QDialog(parent), paths_(paths) {
  setWindowTitle("Export Paths");
  setMinimumWidth(360);
  setModal(true);

  QVBoxLayout* root = new QVBoxLayout(this);
  root->setContentsMargins(12, 12, 12, 12);
  root->setSpacing(10);

  // ---- 标题 ----
  QLabel* title = new QLabel("Select paths to export:", this);
  root->addWidget(title);

  // ---- 全选 / 取消全选 ----
  QHBoxLayout* selectBar = new QHBoxLayout();
  QPushButton* btnAll = new QPushButton("Select All", this);
  QPushButton* btnNone = new QPushButton("Deselect All", this);
  selectBar->addWidget(btnAll);
  selectBar->addWidget(btnNone);
  selectBar->addStretch();
  root->addLayout(selectBar);

  connect(btnAll, &QPushButton::clicked, this, &MapPathExportDialog::SlotSelectAll);
  connect(btnNone, &QPushButton::clicked, this,
          &MapPathExportDialog::SlotDeselectAll);

  // ---- 路径列表 ----
  QWidget* listWidget = new QWidget(this);
  QVBoxLayout* listLayout = new QVBoxLayout(listWidget);
  listLayout->setContentsMargins(0, 0, 0, 0);
  listLayout->setSpacing(4);

  for (const auto& item : paths_) {
    QCheckBox* cb = new QCheckBox(
        QString("%1  (%2)").arg(item.name, FormatLength(item.lengthMeters)),
        listWidget);
    cb->setProperty("path_id", qint64(item.id));
    cb->setChecked(true);
    listLayout->addWidget(cb);
    checkboxes_.append(cb);
  }
  listLayout->addStretch();

  QScrollArea* scroll = new QScrollArea(this);
  scroll->setWidget(listWidget);
  scroll->setWidgetResizable(true);
  scroll->setMaximumHeight(300);
  scroll->setFrameShape(QFrame::NoFrame);
  root->addWidget(scroll);

  // ---- 格式选择 ----
  QHBoxLayout* formatBar = new QHBoxLayout();
  QLabel* lblFmt = new QLabel("Format:", this);
  combo_format_ = new QComboBox(this);
  combo_format_->addItem("JSON (*.json)", "json");
  formatBar->addWidget(lblFmt);
  formatBar->addWidget(combo_format_);
  formatBar->addStretch();
  root->addLayout(formatBar);

  // ---- 按钮 ----
  QHBoxLayout* btnBar = new QHBoxLayout();
  QPushButton* btnCancel = new QPushButton("Cancel", this);
  QPushButton* btnExport = new QPushButton("Export", this);
  btnExport->setDefault(true);
  btnBar->addStretch();
  btnBar->addWidget(btnCancel);
  btnBar->addWidget(btnExport);
  root->addLayout(btnBar);

  connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(btnExport, &QPushButton::clicked, this, &QDialog::accept);
}

QVector<quint64> MapPathExportDialog::selectedIds() const {
  QVector<quint64> ids;
  for (auto* cb : checkboxes_) {
    if (cb->isChecked()) ids.append(cb->property("path_id").toULongLong());
  }
  return ids;
}

QString MapPathExportDialog::format() const {
  return combo_format_->currentData().toString();
}

void MapPathExportDialog::SlotSelectAll() {
  for (auto* cb : checkboxes_) cb->setChecked(true);
}

void MapPathExportDialog::SlotDeselectAll() {
  for (auto* cb : checkboxes_) cb->setChecked(false);
}