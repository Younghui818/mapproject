#include "MapPathWidget.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>

namespace {
QString FormatLength(double meters) {
  // 根据距离量级选择 cm、m 或 km，供路径列表显示。
  if (meters < 1.0) return QString::number(meters * 100.0, 'f', 1) + " cm";
  if (meters < 1000.0) return QString::number(meters, 'f', 1) + " m";
  return QString::number(meters / 1000.0, 'f', 2) + " km";
}
}  // namespace

MapPathWidget::MapPathWidget(MapView* w2d_view, Map3DWidget* w3d_widget,
                             QWidget* parent)
    : QWidget(parent), map_view_(w2d_view), w3d_widget_(w3d_widget) {
  InitUI();
  SignalLink();
  UpdatePathColorButton();
  ApplyPathStyle();
}

void MapPathWidget::InitUI() {
  this->setObjectName("widget_path_style");
  QVBoxLayout* widget_path_style_layout = new QVBoxLayout(this);
  widget_path_style_layout->setContentsMargins(5, 5, 5, 5);
  widget_path_style_layout->setSpacing(6);
  // 线宽
  QWidget* w_line_width = new QWidget(this);
  QHBoxLayout* h_line_width = new QHBoxLayout(w_line_width);
  h_line_width->setContentsMargins(0, 0, 0, 0);
  h_line_width->setSpacing(6);
  QLabel* label_line_width = new QLabel("width", w_line_width);
  slider_path_width_ = new QSlider(Qt::Orientation::Horizontal, w_line_width);
  slider_path_width_->setRange(1, 20);
  slider_path_width_->setValue(path_width_);
  label_path_width_ = new QLabel(w_line_width);
  label_path_width_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  label_path_width_->setText(QString::number(path_width_));
  h_line_width->addWidget(label_line_width);
  h_line_width->addWidget(slider_path_width_, 1);
  h_line_width->addWidget(label_path_width_);
  widget_path_style_layout->addWidget(w_line_width);
  // 颜色
  QWidget* w_line_color = new QWidget(this);
  QHBoxLayout* h_line_color = new QHBoxLayout(w_line_color);
  h_line_color->setContentsMargins(0, 0, 0, 0);
  h_line_color->setSpacing(6);
  QLabel* label_line_color = new QLabel("color", w_line_color);
  btn_path_color_ = new QPushButton(w_line_color);
  btn_path_color_->setObjectName("btnPathColor");
  btn_path_color_->setCursor(Qt::PointingHandCursor);
  btn_path_color_->setToolTip("click to pick path color");
  h_line_color->addWidget(label_line_color);
  h_line_color->addStretch(1);
  h_line_color->addWidget(btn_path_color_);
  widget_path_style_layout->addWidget(w_line_color);
  // 是否同步应用到已保存路径
  cbx_apply_all_paths_ = new QCheckBox("apply to saved", this);
  cbx_apply_all_paths_->setToolTip(
      "also restyle the paths that are already saved");
  widget_path_style_layout->addWidget(cbx_apply_all_paths_);

  // 添加管理路径相关控件
  widget_path_style_layout->addSpacing(10);
  cbx_mouse_add_point_ = new QCheckBox(this);
  cbx_mouse_add_point_->setText("add path");
  widget_path_style_layout->addWidget(cbx_mouse_add_point_);

  // End path button
  btn_end_path_ = new QPushButton("End Path", this);
  widget_path_style_layout->addWidget(btn_end_path_);

  btn_clear_all_path_ = new QPushButton("clear all", this);
  widget_path_style_layout->addWidget(btn_clear_all_path_);

  QHBoxLayout* btn_export_layout = new QHBoxLayout(this);
  btn_export_layout->setContentsMargins(0, 0, 0, 0);
  btn_export_layout->setSpacing(6);
  btn_export_path_ = new QPushButton("export", this);
  btn_export_layout->addWidget(btn_export_path_);
  btn_import_path_ = new QPushButton("import", this);
  btn_export_layout->addWidget(btn_import_path_);
  widget_path_style_layout->addLayout(btn_export_layout);

  // 路径列表容器
  paths_list_widget_ = new QWidget(this);
  paths_list_layout_ = new QVBoxLayout(paths_list_widget_);
  paths_list_layout_->setContentsMargins(2, 2, 2, 2);
  paths_list_layout_->setSpacing(4);
  widget_path_style_layout->addWidget(paths_list_widget_);
}

void MapPathWidget::SignalLink() {
  connect(cbx_mouse_add_point_, &QCheckBox::stateChanged, this,
          &MapPathWidget::SlotStartMouseAddPathChecked);
  connect(btn_clear_all_path_, &QPushButton::clicked, this,
          &MapPathWidget::SlotClearAllPath);
  connect(btn_end_path_, &QPushButton::clicked, this,
          &MapPathWidget::SlotEndPath);
  connect(btn_export_path_, &QPushButton::clicked, this,
          &MapPathWidget::SlotExportPathClicked);
  connect(btn_import_path_, &QPushButton::clicked, this,
          &MapPathWidget::SlotImportPathClicked);

  connect(slider_path_width_, &QSlider::valueChanged, this,
          &MapPathWidget::SlotPathWidthChanged);
  connect(btn_path_color_, &QPushButton::clicked, this,
          &MapPathWidget::SlotPickPathColor);
  connect(cbx_apply_all_paths_, &QCheckBox::stateChanged, this,
          &MapPathWidget::SlotApplyAllPathsStyleChanged);

  // 监听地图视图的路径保存信号。
  connect(map_view_, &MapView::SignalPathSaved, this,
          &MapPathWidget::SlotOnPathSaved);
  connect(w3d_widget_, &Map3DWidget::SignalPathSaved, this,
          &MapPathWidget::SlotOnPathSaved);
  connect(w3d_widget_, &Map3DWidget::SignalPathAnimationFinished, this,
          [this]() {
            // 恢复所有动画按钮
            for (auto* btn : findChildren<QPushButton*>("btnAnim")) {
              btn->setText("start");
              btn->setEnabled(true);
            }
          });
}

void MapPathWidget::SlotStartMouseAddPathChecked(int state) {
  // 同步切换 2D 和 3D 的路径编辑模式；两种模式只允许一个当前编辑状态。
  bool enable = (state != 0);
  if (!map_3d_status_) {
    if (map_view_) {
      map_view_->SlotSetAddingPath(enable);
    }
  } else {
    if (w3d_widget_) {
      w3d_widget_->SlotSetAddingPath(enable);
    }
  }
}

void MapPathWidget::SlotClearAllPath() {
  // 清理两个视图中的当前路径和已保存路径，并刷新路径列表。
  if (map_view_) {
    if (!map_3d_status_) {
      if (map_view_) {
        map_view_->SlotClearAllPaths();
      }
    } else {
      if (w3d_widget_) {
        w3d_widget_->SlotClearAllPaths();
      }
    }

    // 清空侧边 UI
    QLayoutItem* child;
    while ((child = paths_list_layout_->takeAt(0)) != nullptr) {
      if (child->widget()) {
        child->widget()->deleteLater();
      }
      delete child;
    }
  }
  saved_path_infos_.clear();
}

void MapPathWidget::SlotEndPath() {
  // 主动结束当前路径并请求两个视图分别保存其当前路径。
  if (!map_view_) return;
  if (!map_3d_status_) {
    if (map_view_) {
      map_view_->SlotSaveCurrentPath(QString());
    }
  } else {
    if (w3d_widget_) {
      w3d_widget_->SlotSaveCurrentPath(QString());
    }
  }

  cbx_mouse_add_point_->setChecked(false);
  // PathSaved 信号会通知并添加侧边 UI
}

void MapPathWidget::SlotOnPathSaved(quint64 id, const QString& name,
                                    double lengthMeters) {
  saved_path_infos_.append({id, name, lengthMeters});

  const int rowIndex = next_row_index_++;

  QWidget* row = new QWidget(paths_list_widget_);
  row->setObjectName("pathRow");

  QHBoxLayout* hl = new QHBoxLayout(row);
  hl->setContentsMargins(2, 2, 2, 2);

  QLabel* lbl =
      new QLabel(QString("%1    <span style='color:#47A9FF;'>%2</span>")
                     .arg(name, FormatLength(lengthMeters)),
                 row);
  lbl->setTextFormat(Qt::RichText);
  lbl->setToolTip(QString("%1 — %2 m").arg(name).arg(lengthMeters, 0, 'f', 2));

  // ★ 动画按钮
  QPushButton* btnAnim = new QPushButton("start", row);
  btnAnim->setObjectName("btnAnim");
  btnAnim->setToolTip("Play path grow animation");
  btnAnim->setCursor(Qt::PointingHandCursor);
  btnAnim->setStyleSheet("color:white; size:8px;");
  QPushButton* del = new QPushButton("Delete", row);
  del->setObjectName("btnDelete");

  hl->addWidget(lbl);
  hl->addStretch();
  hl->addWidget(btnAnim);
  hl->addWidget(del);
  paths_list_layout_->addWidget(row);

  path_row_index_[id] = rowIndex;

  // 删除
  connect(del, &QPushButton::clicked, this, [this, id, row]() {
    for (int i = 0; i < saved_path_infos_.size(); ++i) {
      if (saved_path_infos_[i].id == id) {
        saved_path_infos_.removeAt(i);
        break;
      }
    }

    if (!map_3d_status_) {
      if (map_view_) map_view_->SlotRemoveSavedPathById(id);
    } else {
      if (w3d_widget_) w3d_widget_->SlotRemoveSavedPathById(id);
    }
    path_row_index_.remove(id);
    row->deleteLater();
  });

  // ★ 动画按钮点击
  connect(btnAnim, &QPushButton::clicked, this, [this, id, btnAnim]() {
    // 找到该 path 在 saved_paths_ 中的索引
    // 通过 Map3DWidget → Scene3DRenderer 的 saved_path_ids_ 查找
    const int idx = w3d_widget_->GetPathIndexById(id);
    if (idx >= 0) {
      w3d_widget_->PlayPathAnimation(idx);
      btnAnim->setText(QStringLiteral("running..."));
      btnAnim->setEnabled(false);
      // 动画结束后恢复按钮状态
      QTimer::singleShot(2000, btnAnim, [btnAnim]() {
        btnAnim->setText("start");
        btnAnim->setEnabled(true);
      });
    }
  });
}

void MapPathWidget::SlotPathWidthChanged(int width) {
  path_width_ = width;
  label_path_width_->setText(QString::number(width));
  ApplyPathStyle();
}

void MapPathWidget::SlotPickPathColor() {
  QColor c = QColorDialog::getColor(path_color_, this, "Path Color",
                                    QColorDialog::ShowAlphaChannel);
  if (!c.isValid()) return;
  path_color_ = c;
  UpdatePathColorButton();
  ApplyPathStyle();
}

void MapPathWidget::SlotApplyAllPathsStyleChanged(int state) {
  if (state != 0) ApplyPathStyle();  // 勾选瞬间同步已有路径
}

void MapPathWidget::ApplyPathStyle() {
  // 将主窗口当前路径样式同时下发到 2D 和 3D 视图。
  const bool all = cbx_apply_all_paths_->isChecked();

  // 两个视图都下发，切换 2D/3D 后样式保持一致
  if (map_view_) {
    if (all)
      map_view_->SetAllPathsStyle(path_width_, path_color_);
    else
      map_view_->SetPathStyle(path_width_, path_color_);
  }
  if (w3d_widget_) {
    if (all)
      w3d_widget_->SetAllPathsStyle(path_width_, path_color_);
    else
      w3d_widget_->SetPathStyle(path_width_, path_color_);
  }
}

void MapPathWidget::UpdatePathColorButton() {
  btn_path_color_->setStyleSheet(
      QString("QPushButton#btnPathColor{"
              "background-color:%1;"
              "border:1px solid #454B55;"
              "border-radius:4px;}"
              "QPushButton#btnPathColor:hover{border-color:#47A9FF;}")
          .arg(path_color_.name(QColor::HexArgb)));
}

void MapPathWidget::SlotExportPathClicked() { ShowExportDialog(); }

void MapPathWidget::SlotImportPathClicked() {
  // 系统文件选择对话框
  QString filePath =
      QFileDialog::getOpenFileName(this, "Import Paths", QDir::homePath(),
                                   "JSON Files (*.json);;All Files (*)");

  if (filePath.isEmpty()) return;

  QVector<quint64> importedIds;
  int count = w3d_widget_->ImportPaths(filePath, importedIds);

  if (count <= 0) {
    QMessageBox::warning(
        this, "Import Paths",
        "Failed to import paths.\n"
        "The file may be invalid or contain no valid path data.");
    return;
  }
  QMessageBox::information(
      this, "Import Paths",
      QString("Successfully imported %1 path(s).").arg(count));
}

void MapPathWidget::ShowExportDialog() {
  if (saved_path_infos_.isEmpty()) {
    QMessageBox::information(this, "Export Paths", "No saved paths to export.");
    return;
  }

  // 构造对话框数据
  QVector<PathExportItem> items;
  for (const auto& info : saved_path_infos_) {
    items.append({info.id, info.name, info.lengthMeters});
  }

  MapPathExportDialog dlg(items, this);
  if (dlg.exec() != QDialog::Accepted) return;

  QVector<quint64> selectedIds = dlg.selectedIds();
  if (selectedIds.isEmpty()) {
    QMessageBox::warning(this, "Export Paths", "No paths selected.");
    return;
  }

  // 选文件
  QString filter = "JSON Files (*.json)";
  QString filePath = QFileDialog::getSaveFileName(this, "Export Paths",
                                                  "paths_export.json", filter);
  if (filePath.isEmpty()) return;

  if (!filePath.endsWith(".json", Qt::CaseInsensitive)) filePath += ".json";

  // 调用导出
  bool ok = w3d_widget_->ExportPaths(selectedIds, filePath);
  if (ok) {
    QMessageBox::information(
        this, "Export Paths",
        QString("Successfully exported %1 path(s).").arg(selectedIds.size()));
  } else {
    QMessageBox::warning(this, "Export Paths", "Failed to export paths.");
  }
}