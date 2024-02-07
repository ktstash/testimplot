#include "plot_bar_stack_util.h"
#include "implot_internal.h"

namespace
{
    // Helper function to get color base on value
    template<typename T>
    ImU32 get_color_based_on_value(T value) {
        ImColor color; // Declare an ImColor variable

        // Check if the type is bool
        if constexpr (std::is_same<T, bool>::value) {
            color = value ? ImColor(0, 0, 255, 255) : ImColor(255, 0, 0, 255); // Blue for true, Red for false
        }
        // Check if the type is int
        else if constexpr (std::is_same<T, int>::value) {
            if (value > 0) {
                color = ImColor(128, 0, 128, 255); // Purple for int > 0
            }
            else if (value < 0) {
                color = ImColor(255, 255, 0, 255); // Yellow for int < 0
            }
            else {
                color = ImColor(0, 0, 0, 255); // Black for int == 0, as an edge case
            }
        }
        // Default case for unsupported types
        else {
            color = ImColor(255, 255, 255, 255); // White for unsupported types
        }

        return color; // Implicitly converts ImColor to ImU32
    }
}

// This section is copied from Utils in implot_items.cpp
//-----------------------------------------------------------------------------
// [SECTION] Utils
//-----------------------------------------------------------------------------
// Calc maximum index size of ImDrawIdx
template <typename T>
struct MaxIdx { static const unsigned int value; };
template <> const unsigned int MaxIdx<unsigned short>::value = 65535;
template <> const unsigned int MaxIdx<unsigned int>::value = 4294967295;


// This section is copied from Indexers section in implot_items.cpp
//-----------------------------------------------------------------------------
// [SECTION] Indexers
//-----------------------------------------------------------------------------
template <typename T>
T index_data(const T* data, int idx, int count, int offset, int stride) {
    const int s = ((offset == 0) << 0) | ((stride == sizeof(T)) << 1);
    switch (s) {
    case 3: return data[idx];
    case 2: return data[(offset + idx) % count];
    case 1: return *(const T*)(const void*)((const unsigned char*)data + (size_t)((idx)) * stride);
    case 0: return *(const T*)(const void*)((const unsigned char*)data + (size_t)((offset + idx) % count) * stride);
    default: return T(0);
    }
}

template <typename T>
struct IndexerIdx {
    IndexerIdx(const T* data, int count, int offset = 0, int stride = sizeof(T)) :
        data(data),
        count(count),
        offset(count ? ImPosMod(offset, count) : 0),
        stride(stride)
    { }
    template <typename I>double operator()(I idx) const {
        return (double)index_data(data, idx, count, offset, stride);
    }
    const T* data;
    int count;
    int offset;
    int stride;
};

template <typename _Indexer1, typename _Indexer2>
struct IndexerAdd {
    IndexerAdd(const _Indexer1& indexer1, const _Indexer2& indexer2, double scale1 = 1, double scale2 = 1)
        : indexer1(indexer1),
        indexer2(indexer2),
        scale1(scale1),
        scale2(scale2),
        count(ImMin(this.indexer1.count, this.indexer2.count))
    { }
    template <typename I>double operator()(I idx) const {
        return scale1 * indexer1(idx) + scale2 * indexer2(idx);
    }
    const _Indexer1& indexer1;
    const _Indexer2& indexer2;
    double scale1;
    double scale2;
    int count;
};

struct IndexerLin {
    IndexerLin(double m, double b) : m(m), b(b) { }
    template <typename I> double operator()(I idx) const {
        return m * idx + b;
    }
    const double m;
    const double b;
};

struct IndexerConst {
    IndexerConst(double ref) : ref(ref) { }
    template <typename I>double operator()(I) const { return ref; }
    const double ref;
};

// This section is copied from GetterXY in implot_items.cpp
//-----------------------------------------------------------------------------
// [SECTION] Getters
//-----------------------------------------------------------------------------
template <typename _IndexerX, typename _IndexerY>
struct GetterXY {
    GetterXY(_IndexerX x, _IndexerY y, int count) : indxer_x(x), indxer_y(y), count(count) { }
    template <typename I>ImPlotPoint operator()(I idx) const {
        return ImPlotPoint(indxer_x(idx), indxer_y(idx));
    }
    const _IndexerX indxer_x;
    const _IndexerY indxer_y;
    const int count;
};

// This section is copied from Fitters in implot_items.cpp
//-----------------------------------------------------------------------------
// [SECTION] Fitters
//-----------------------------------------------------------------------------
template <typename _Getter1, typename _Getter2>
struct FitterBarH {
    FitterBarH(const _Getter1& getter1, const _Getter2& getter2, double height) :
        getter1(getter1),
        getter2(getter2),
        half_height(height * 0.5)
    { }
    // It has to be uppercase, otherwise implot_internal.h will fail at BeginItemEX
    void Fit(ImPlotAxis& x_axis, ImPlotAxis& y_axis) const {
        int count = ImMin(getter1.count, getter2.count);
        for (int i = 0; i < count; ++i) {
            ImPlotPoint p1 = getter1(i); p1.y -= half_height;
            ImPlotPoint p2 = getter2(i); p2.y += half_height;
            x_axis.ExtendFitWith(y_axis, p1.x, p1.y);
            y_axis.ExtendFitWith(x_axis, p1.y, p1.x);
            x_axis.ExtendFitWith(y_axis, p2.x, p2.y);
            y_axis.ExtendFitWith(x_axis, p2.y, p2.x);
        }
    }
    const _Getter1& getter1;
    const _Getter2& getter2;
    const double    half_height;
};

// This section is copied from Item Utils in implot_items.cpp
//-----------------------------------------------------------------------------
// [SECTION] Item Utils
//-----------------------------------------------------------------------------

bool is_item_hidden(const char* label_id) {
    ImPlotItem* item = ImPlot::GetItem(label_id);
    return item != nullptr && !item->Show;
}

void prim_rect_fill(ImDrawList& draw_list, const ImVec2& Pmin, const ImVec2& Pmax, ImU32 col, const ImVec2& uv) {
    draw_list._VtxWritePtr[0].pos = Pmin;
    draw_list._VtxWritePtr[0].uv = uv;
    draw_list._VtxWritePtr[0].col = col;
    draw_list._VtxWritePtr[1].pos = Pmax;
    draw_list._VtxWritePtr[1].uv = uv;
    draw_list._VtxWritePtr[1].col = col;
    draw_list._VtxWritePtr[2].pos.x = Pmin.x;
    draw_list._VtxWritePtr[2].pos.y = Pmax.y;
    draw_list._VtxWritePtr[2].uv = uv;
    draw_list._VtxWritePtr[2].col = col;
    draw_list._VtxWritePtr[3].pos.x = Pmax.x;
    draw_list._VtxWritePtr[3].pos.y = Pmin.y;
    draw_list._VtxWritePtr[3].uv = uv;
    draw_list._VtxWritePtr[3].col = col;
    draw_list._VtxWritePtr += 4;
    draw_list._IdxWritePtr[0] = (ImDrawIdx)(draw_list._VtxCurrentIdx);
    draw_list._IdxWritePtr[1] = (ImDrawIdx)(draw_list._VtxCurrentIdx + 1);
    draw_list._IdxWritePtr[2] = (ImDrawIdx)(draw_list._VtxCurrentIdx + 2);
    draw_list._IdxWritePtr[3] = (ImDrawIdx)(draw_list._VtxCurrentIdx);
    draw_list._IdxWritePtr[4] = (ImDrawIdx)(draw_list._VtxCurrentIdx + 1);
    draw_list._IdxWritePtr[5] = (ImDrawIdx)(draw_list._VtxCurrentIdx + 3);
    draw_list._IdxWritePtr += 6;
    draw_list._VtxCurrentIdx += 4;
}

// This section is copied from BeginItem / EndItem in implot_items.cpp
//-----------------------------------------------------------------------------
// [SECTION] BeginItem / EndItem
//-----------------------------------------------------------------------------
void end_item() {
    ImPlotContext& gp = *GImPlot;
    // pop rendering clip rect
    ImPlot::PopPlotClipRect();
    // reset next item data
    gp.NextItemData.Reset();
    // set current item1
    gp.PreviousItem = gp.CurrentItem;
    gp.CurrentItem = nullptr;
}

// This section is copied from Transformers in implot_items.cpp
//-----------------------------------------------------------------------------
// [SECTION] Transformers
//-----------------------------------------------------------------------------

struct Transformer1 {
    Transformer1(double pix_min, double plt_min, double plt_max, double m, double sca_min, double sca_max, ImPlotTransform fwd, void* data) :
        sca_min(sca_min),
        sca_max(sca_max),
        plt_min(plt_min),
        plt_max(plt_max),
        pix_min(pix_min),
        m(m),
        transform_fwd(fwd),
        transform_data(data)
    { }

    template <typename T> float operator()(T p) const {
        if (transform_fwd != nullptr) {
            double s = transform_fwd(p, transform_data);
            double t = (s - sca_min) / (sca_max - sca_min);
            p = plt_min + (plt_max - plt_min) * t;
        }
        return (float)(pix_min + m * (p - plt_min));
    }

    double sca_min, sca_max, plt_min, plt_max, pix_min, m;
    ImPlotTransform transform_fwd;
    void* transform_data;
};

struct Transformer2 {
    Transformer2(const ImPlotAxis& x_axis, const ImPlotAxis& y_axis) :
        t_x(x_axis.PixelMin,
            x_axis.Range.Min,
            x_axis.Range.Max,
            x_axis.ScaleToPixel,
            x_axis.ScaleMin,
            x_axis.ScaleMax,
            x_axis.TransformForward,
            x_axis.TransformData),
        t_y(y_axis.PixelMin,
            y_axis.Range.Min,
            y_axis.Range.Max,
            y_axis.ScaleToPixel,
            y_axis.ScaleMin,
            y_axis.ScaleMax,
            y_axis.TransformForward,
            y_axis.TransformData)
    { }

    Transformer2(const ImPlotPlot& plot) :
        Transformer2(plot.Axes[plot.CurrentX], plot.Axes[plot.CurrentY])
    { }

    Transformer2() :
        Transformer2(*GImPlot->CurrentPlot)
    { }

    template <typename P> ImVec2 operator()(const P& plt) const {
        ImVec2 out;
        out.x = t_x(plt.x);
        out.y = t_y(plt.y);
        return out;
    }

    template <typename T> ImVec2 operator()(T x, T y) const {
        ImVec2 out;
        out.x = t_x(x);
        out.y = t_y(y);
        return out;
    }

    Transformer1 t_x;
    Transformer1 t_y;
};

// This section is copied from Renderers in implot_items.cpp
//-----------------------------------------------------------------------------
// [SECTION] Renderers
//-----------------------------------------------------------------------------
struct RendererBase {
    RendererBase(int prims, int idx_consumed, int vtx_consumed) :
        prims(prims),
        idx_consumed(idx_consumed),
        vtx_consumed(vtx_consumed)
    { }
    const int prims;
    Transformer2 transformer;
    const int idx_consumed;
    const int vtx_consumed;
};


template <class _Getter1, class _Getter2>
struct RendererBarsFillH : RendererBase {
    RendererBarsFillH(const _Getter1& getter1, const _Getter2& getter2, ImU32 col, double height) :
        RendererBase(ImMin(getter1.count, getter1.count), 6, 4),
        getter1(getter1),
        getter2(getter2),
        col(col),
        half_height(height / 2)
    {}
    void Init(ImDrawList& draw_list) const {
        uv = draw_list._Data->TexUvWhitePixel;
    }
    bool Render(ImDrawList& draw_list, const ImRect& cull_rect, int prim) const {
        ImPlotPoint p1 = getter1(prim);
        ImPlotPoint p2 = getter2(prim);
        p1.y += half_height;
        p2.y -= half_height;
        ImVec2 P1 = this->transformer(p1);
        ImVec2 P2 = this->transformer(p2);
        float height_px = ImAbs(P1.y - P2.y);
        if (height_px < 1.0f) {
            P1.y += P1.y > P2.y ? (1 - height_px) / 2 : (height_px - 1) / 2;
            P2.y += P2.y > P1.y ? (1 - height_px) / 2 : (height_px - 1) / 2;
        }
        ImVec2 p_min = ImMin(P1, P2);
        ImVec2 p_max = ImMax(P1, P2);
        if (!cull_rect.Overlaps(ImRect(p_min, p_max)))
            return false;
        prim_rect_fill(draw_list, p_min, p_max, col, uv);
        return true;
    }
    const _Getter1& getter1;
    const _Getter2& getter2;
    const ImU32 col;
    const double half_height;
    mutable ImVec2 uv;
};

// This section is copied from RenderPrimitives in implot_items.cpp
//-----------------------------------------------------------------------------
// [SECTION] RenderPrimitives
//-----------------------------------------------------------------------------
template <class _Renderer>
void RenderPrimitivesEx(const _Renderer& renderer, ImDrawList& draw_list, const ImRect& cull_rect) {
    unsigned int prims = renderer.prims;
    unsigned int prims_culled = 0;
    unsigned int idx = 0;
    renderer.Init(draw_list);
    while (prims) {
        // find how many can be reserved up to end of current draw command's limit
        unsigned int cnt = ImMin(prims, (MaxIdx<ImDrawIdx>::value - draw_list._VtxCurrentIdx) / renderer.vtx_consumed);
        // make sure at least this many elements can be rendered to avoid situations where at the end of buffer this slow path is not taken all the time
        if (cnt >= ImMin(64u, prims)) {
            if (prims_culled >= cnt)
                prims_culled -= cnt; // reuse previous reservation
            else {
                // add more elements to previous reservation
                draw_list.PrimReserve((cnt - prims_culled) * renderer.idx_consumed, (cnt - prims_culled) * renderer.vtx_consumed);
                prims_culled = 0;
            }
        }
        else
        {
            if (prims_culled > 0) {
                draw_list.PrimUnreserve(prims_culled * renderer.idx_consumed, prims_culled * renderer.vtx_consumed);
                prims_culled = 0;
            }
            cnt = ImMin(prims, (MaxIdx<ImDrawIdx>::value - 0/*draw_list._VtxCurrentIdx*/) / renderer.vtx_consumed);
            // reserve new draw command
            draw_list.PrimReserve(cnt * renderer.idx_consumed, cnt * renderer.vtx_consumed);
        }
        prims -= cnt;
        for (unsigned int ie = idx + cnt; idx != ie; ++idx) {
            if (!renderer.Render(draw_list, cull_rect, idx))
                prims_culled++;
        }
    }
    if (prims_culled > 0)
        draw_list.PrimUnreserve(prims_culled * renderer.idx_consumed, prims_culled * renderer.vtx_consumed);
}

template <template <class, class> class _Renderer, class _Getter1, class _Getter2, typename ...Args>
void RenderPrimitives2(const _Getter1& getter1, const _Getter2& getter2, Args... args) {
    ImDrawList& draw_list = *ImPlot::GetPlotDrawList();
    const ImRect& cull_rect = ImPlot::GetCurrentPlot()->PlotRect;
    RenderPrimitivesEx(_Renderer<_Getter1, _Getter2>(getter1, getter2, args...), draw_list, cull_rect);
}

//-----------------------------------------------------------------------------
// [SECTION] New function for Ploting continous bar stack
//-----------------------------------------------------------------------------

// Copied and modified from PlotBarsHEx in implot_items.cpp
template <typename Getter1, typename Getter2>
void plot_bars_stack_ex(std::string label_id, const Getter1& getter1, const Getter2& getter2, double height, ImU32 bar_color, ImPlotBarsFlags flags) {
    if (ImPlot::BeginItemEx(label_id.c_str(), FitterBarH<Getter1, Getter2>(getter1, getter2, height), flags, ImPlotCol_Fill)) {
        if (getter1.count <= 0 || getter2.count <= 0) {
            end_item();
            return;
        }
        RenderPrimitives2<RendererBarsFillH>(getter1, getter2, bar_color, height);
        end_item();
    }
}

// Copied and modified from PlotBarGroups in implot_items.cpp
template <typename T1, typename T2>
void plot_bar_stack(std::string label_id, const T1* bar_length, std::vector<T2> bar_value, int item_count, double group_size, double shift, ImPlotBarGroupsFlags flags) {
    const bool horz = ImHasFlag(flags, ImPlotBarGroupsFlags_Horizontal);
    const bool stack = ImHasFlag(flags, ImPlotBarGroupsFlags_Stacked);
    int group_count = 1;
    ImPlot::SetupLock();
    ImPlotContext& gp = *GImPlot;
    gp.TempDouble1.resize(4 * group_count);
    double* temp = gp.TempDouble1.Data;
    double* neg = &temp[0];
    double* pos = &temp[group_count];
    double* curr_min = &temp[group_count * 2];
    double* curr_max = &temp[group_count * 3];
    for (int g = 0; g < 2; ++g)
        temp[g] = 0;
    if (horz) {
        for (int i = 0; i < item_count; ++i) {
            double v = (double)bar_length[i];
            if (!is_item_hidden(label_id.c_str())) {

                if (v > 0) {
                    curr_min[0] = pos[0];
                    curr_max[0] = curr_min[0] + v;
                    pos[0] += v;
                }
                else {
                    curr_max[0] = neg[0];
                    curr_min[0] = curr_max[0] + v;
                    neg[0] += v;
                }
            }
            GetterXY<IndexerIdx<double>, IndexerLin> getter1(IndexerIdx<double>(curr_min, 1), IndexerLin(1.0, shift), 1);
            GetterXY<IndexerIdx<double>, IndexerLin> getter2(IndexerIdx<double>(curr_max, 1), IndexerLin(1.0, shift), 1);
            auto color_for_v = get_color_based_on_value<T2>(bar_value[i]);
            plot_bars_stack_ex(label_id, getter1, getter2, group_size, color_for_v, 0);
        }
    }
}

// Explicit template instantiation for the types you expect to be used
// This is necessary because the template implementation is in the .cpp file
template void plot_bar_stack<uint64_t, bool>(std::string label_id, const uint64_t* bar_length, std::vector<bool> bar_value, int item_count, double group_size, double shift, ImPlotBarGroupsFlags flags);
template void plot_bar_stack<uint64_t, int>(std::string label_id, const uint64_t* bar_length, std::vector<int> bar_value, int item_count, double group_size, double shift, ImPlotBarGroupsFlags flags);
