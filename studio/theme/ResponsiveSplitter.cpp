#include "theme/ResponsiveSplitter.hpp"

#include <QEvent>
#include <QObject>
#include <QSplitter>
#include <QTimer>

#include <algorithm>
#include <numeric>
#include <vector>

namespace aur::theme
{
    namespace
    {
        class SplitterBinder final : public QObject
        {
        public:
            SplitterBinder(QSplitter *splitter, std::vector<double> ratios)
                : QObject(splitter), splitter_(splitter), ratios_(std::move(ratios))
            {
                const int count = std::min(splitter_->count(), static_cast<int>(ratios_.size()));
                for (int i = 0; i < count; ++i)
                {
                    splitter_->setStretchFactor(i, std::max(1, static_cast<int>(ratios_[i] * 100)));
                    splitter_->setCollapsible(i, false);
                }
                splitter_->installEventFilter(this);
                connect(splitter_, &QSplitter::splitterMoved, this, [this](int, int) {
                    if (applying_) return;
                    const QList<int> current = splitter_->sizes();
                    const int total = std::accumulate(current.begin(), current.end(), 0);
                    if (total <= 0 || current.size() != static_cast<qsizetype>(ratios_.size()))
                        return;
                    for (int i = 0; i < current.size(); ++i)
                        ratios_[static_cast<std::size_t>(i)] =
                            static_cast<double>(current[i]) / static_cast<double>(total);
                });
                QTimer::singleShot(0, this, [this]() { apply(); });
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override
            {
                if (watched == splitter_ &&
                    (event->type() == QEvent::Resize || event->type() == QEvent::Show))
                {
                    apply();
                }
                return QObject::eventFilter(watched, event);
            }

        private:
            void apply()
            {
                if (!splitter_ || ratios_.empty()) return;
                const int span = splitter_->orientation() == Qt::Horizontal ? splitter_->width()
                                                                            : splitter_->height();
                if (span <= 60) return;
                const int count = std::min(splitter_->count(), static_cast<int>(ratios_.size()));
                const int handles = std::max(0, count - 1) * splitter_->handleWidth();
                const int available = std::max(count, span - handles);
                QList<int> target;
                int assigned = 0;
                for (int i = 0; i < count; ++i)
                {
                    const int slice = (i + 1 == count)
                                          ? std::max(1, available - assigned)
                                          : std::max(1, static_cast<int>(std::lround(
                                                            available * ratios_[static_cast<std::size_t>(i)])));
                    target.append(slice);
                    assigned += slice;
                }
                applying_ = true;
                splitter_->setSizes(target);
                applying_ = false;
            }

            QSplitter *splitter_ = nullptr;
            std::vector<double> ratios_;
            bool applying_ = false;
        };
    }

    void bindResponsiveSplitter(QSplitter *splitter, std::initializer_list<int> weights)
    {
        if (!splitter || weights.size() == 0) return;
        const double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
        if (sum <= 0.0) return;
        std::vector<double> ratios;
        ratios.reserve(weights.size());
        for (int w : weights) ratios.push_back(static_cast<double>(w) / sum);
        new SplitterBinder(splitter, std::move(ratios));
    }

}
