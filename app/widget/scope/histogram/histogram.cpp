/***

  Olive - Non-Linear Video Editor
  Copyright (C) 2019 Olive Team

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

***/

#include "histogram.h"

#include <QPainter>
#include <QtMath>
#include <QVector2D>

#include "common/qtutils.h"
#include "node/node.h"

OLIVE_NAMESPACE_ENTER

HistogramScope::HistogramScope(QWidget* parent) :
  ScopeBase(parent)
{
}

HistogramScope::~HistogramScope()
{
  OnDestroy();
}

void HistogramScope::OnInit()
{
  ScopeBase::OnInit();

  ShaderCode secondary_code(FileFunctions::ReadFileAsString(":/shaders/rgbhistogram_secondary.frag"),
                            FileFunctions::ReadFileAsString(":/shaders/rgbhistogram.vert"));
  pipeline_secondary_ = renderer()->CreateNativeShader(secondary_code);
}

void HistogramScope::OnDestroy()
{
  ScopeBase::OnDestroy();

  pipeline_secondary_.clear();
  texture_row_sums_ = nullptr;
}

ShaderCode HistogramScope::GenerateShaderCode()
{
  return ShaderCode(FileFunctions::ReadFileAsString(":/shaders/rgbhistogram.frag"),
                    FileFunctions::ReadFileAsString(":/shaders/default.vert"));
}

void HistogramScope::DrawScope(Renderer::TexturePtr managed_tex, QVariant pipeline)
{
  float histogram_scale = 0.80f;
  // This value is eyeballed for usefulness. Until we have a geometry
  // shader approach, it is impossible to normalize against a peak
  // sum of image values.
  float histogram_base = 2.5f;
  float histogram_power = 1.0f / histogram_base;

  ShaderJob shader_job;

  shader_job.InsertValue(QStringLiteral("viewport"), ShaderValue(QVector2D(width(), height()), NodeParam::kVec2));
  shader_job.InsertValue(QStringLiteral("histogram_scale"), ShaderValue(histogram_scale, NodeParam::kFloat));
  shader_job.InsertValue(QStringLiteral("histogram_power"), ShaderValue(histogram_power, NodeParam::kFloat));

  if (!texture_row_sums_
      || texture_row_sums_->width() != this->width()
      || texture_row_sums_->height() != this->height()) {
    texture_row_sums_ = renderer()->CreateTexture(VideoParams(width(), height(), managed_tex->format()));
  }

  // Draw managed texture to a sums texture
  shader_job.InsertValue(QStringLiteral("ove_maintex"), ShaderValue(QVariant::fromValue(managed_tex), NodeParam::kTexture));
  renderer()->BlitToTexture(pipeline, shader_job, texture_row_sums_.get());

  // Draw sums into a histogram
  shader_job.InsertValue(QStringLiteral("ove_maintex"), ShaderValue(QVariant::fromValue(texture_row_sums_), NodeParam::kTexture));
  renderer()->Blit(pipeline_secondary_, shader_job, texture_row_sums_->params());

  // Draw line overlays
  QPainter p(inner_widget());
  QFont font = p.font();
  font.setPixelSize(10);
  QFontMetrics font_metrics = QFontMetrics(font);
  QString label;
  std::vector<float> histogram_increments = {
    0.00,
    0.25,
    0.50,
    1.0
  };

  int histogram_steps = histogram_increments.size();
  QVector<QLine> histogram_lines(histogram_steps + 1);
  int font_x_offset = 0;
  int font_y_offset = font_metrics.capHeight() / 2.0f;

  p.setCompositionMode(QPainter::CompositionMode_Plus);

  p.setPen(QColor(0.0, 0.6 * 255.0, 0.0));
  p.setFont(font);

  float histogram_dim_x = ceil((width() - 1.0) * histogram_scale);
  float histogram_dim_y = ceil((height() - 1.0) * histogram_scale);
  float histogram_start_dim_x =
      ((width() - 1.0) - histogram_dim_x) / 2.0f;
  float histogram_start_dim_y =
      ((height() - 1.0) - histogram_dim_y) / 2.0f;
  float histogram_end_dim_x = (width() - 1.0) - histogram_start_dim_x;

  // for (int i=0; i <= histogram_steps; i++) {
  for(std::vector<float>::iterator it = histogram_increments.begin();
      it != histogram_increments.end(); it++) {
    histogram_lines[it - histogram_increments.begin()].setLine(
          histogram_start_dim_x,
          (histogram_dim_y * pow(1.0 - *it, histogram_base)) +
          histogram_start_dim_y,
          histogram_end_dim_x,
          (histogram_dim_y * pow(1.0 - *it, histogram_base)) +
          histogram_start_dim_y);
    label = QString::number(
          *it * 100, 'f', 1) + "%";
    font_x_offset = QFontMetricsWidth(font_metrics, label) + 4;

    p.drawText(
          histogram_start_dim_x - font_x_offset,
          (histogram_dim_y * pow(1.0 - *it, histogram_base)) +
          histogram_start_dim_y + font_y_offset, label);
  }
  p.drawLines(histogram_lines);
}

OLIVE_NAMESPACE_EXIT
