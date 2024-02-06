// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui\imgui.h"
#include "imgui\backends\imgui_impl_glfw.h"
#include "imgui\backends\imgui_impl_opengl3.h"
#include "imgui\implot.h"
#include "implot_internal.h"
#include <stdio.h>
#include <deque>
#include <string>
#include <string_view>
#include <vector>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers


#include <type_traits>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Calc maximum index size of ImDrawIdx
template <typename T>
struct MaxIdx { static const unsigned int Value; };
template <> const unsigned int MaxIdx<unsigned short>::Value = 65535;
template <> const unsigned int MaxIdx<unsigned int>::Value = 4294967295;

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

// Copied from Indexer in implot_items.cpp
template <typename T>
T IndexData(const T* data, int idx, int count, int offset, int stride) {
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
        Data(data),
        Count(count),
        Offset(count ? ImPosMod(offset, count) : 0),
        Stride(stride)
    { }
    template <typename I>double operator()(I idx) const {
        return (double)IndexData(Data, idx, Count, Offset, Stride);
    }
    const T* Data;
    int Count;
    int Offset;
    int Stride;
};

template <typename _Indexer1, typename _Indexer2>
struct IndexerAdd {
    IndexerAdd(const _Indexer1& indexer1, const _Indexer2& indexer2, double scale1 = 1, double scale2 = 1)
        : Indexer1(indexer1),
        Indexer2(indexer2),
        Scale1(scale1),
        Scale2(scale2),
        Count(ImMin(Indexer1.Count, Indexer2.Count))
    { }
    template <typename I>double operator()(I idx) const {
        return Scale1 * Indexer1(idx) + Scale2 * Indexer2(idx);
    }
    const _Indexer1& Indexer1;
    const _Indexer2& Indexer2;
    double Scale1;
    double Scale2;
    int Count;
};

struct IndexerLin {
    IndexerLin(double m, double b) : M(m), B(b) { }
    template <typename I> double operator()(I idx) const {
        return M * idx + B;
    }
    const double M;
    const double B;
};

struct IndexerConst {
    IndexerConst(double ref) : Ref(ref) { }
    template <typename I>double operator()(I) const { return Ref; }
    const double Ref;
};

// Copied from GetterXY in implot_items.cpp
template <typename _IndexerX, typename _IndexerY>
struct GetterXY {
    GetterXY(_IndexerX x, _IndexerY y, int count) : IndxerX(x), IndxerY(y), Count(count) { }
    template <typename I>ImPlotPoint operator()(I idx) const {
        return ImPlotPoint(IndxerX(idx), IndxerY(idx));
    }
    const _IndexerX IndxerX;
    const _IndexerY IndxerY;
    const int Count;
};

template <typename _Getter1, typename _Getter2>
struct FitterBarH {
    FitterBarH(const _Getter1& getter1, const _Getter2& getter2, double height) :
        Getter1(getter1),
        Getter2(getter2),
        HalfHeight(height * 0.5)
    { }
    void Fit(ImPlotAxis& x_axis, ImPlotAxis& y_axis) const {
        int count = ImMin(Getter1.Count, Getter2.Count);
        for (int i = 0; i < count; ++i) {
            ImPlotPoint p1 = Getter1(i); p1.y -= HalfHeight;
            ImPlotPoint p2 = Getter2(i); p2.y += HalfHeight;
            x_axis.ExtendFitWith(y_axis, p1.x, p1.y);
            y_axis.ExtendFitWith(x_axis, p1.y, p1.x);
            x_axis.ExtendFitWith(y_axis, p2.x, p2.y);
            y_axis.ExtendFitWith(x_axis, p2.y, p2.x);
        }
    }
    const _Getter1& Getter1;
    const _Getter2& Getter2;
    const double    HalfHeight;
};

// Copied from IsItemHidden in implot_items.cpp
bool IsItemHidden(const char* label_id) {
    ImPlotItem* item = ImPlot::GetItem(label_id);
    return item != nullptr && !item->Show;
}

// Ends an item (call only if BeginItem returns true)
void EndItem() {
    ImPlotContext& gp = *GImPlot;
    // pop rendering clip rect
    ImPlot::PopPlotClipRect();
    // reset next item data
    gp.NextItemData.Reset();
    // set current item1
    gp.PreviousItem = gp.CurrentItem;
    gp.CurrentItem = nullptr;
}

// Copied from PlotBarGroups in implot_items.cpp
template <typename T1, typename T2>
void PlotBarStacks(std::string label_id, const T1* bar_length,std::vector<T2> bar_value, int item_count, double group_size, double shift, ImPlotBarGroupsFlags flags) {
    const bool horz = ImHasFlag(flags, ImPlotBarGroupsFlags_Horizontal);
    const bool stack = ImHasFlag(flags, ImPlotBarGroupsFlags_Stacked);
    int group_count = 1;
        ImPlot::SetupLock();
        ImPlotContext& gp = *GImPlot;
        gp.TempDouble1.resize(4*group_count);
        double* temp = gp.TempDouble1.Data;
        double* neg = &temp[0];
        double* pos = &temp[group_count];
        double* curr_min = &temp[group_count*2];
        double* curr_max = &temp[group_count*3];
        for (int g = 0; g < 2; ++g)
            temp[g] = 0;
        if (horz) {
            for (int i = 0; i < item_count; ++i) { 
                double v = (double)bar_length[i];
                if (!IsItemHidden(label_id.c_str())) {
                       
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
                PlotBarsStackEx(label_id, getter1, getter2, group_size, color_for_v, 0);
            }
        }
}

//-----------------------------------------------------------------------------
// [SECTION] Transformers
//-----------------------------------------------------------------------------

struct Transformer1 {
    Transformer1(double pixMin, double pltMin, double pltMax, double m, double scaMin, double scaMax, ImPlotTransform fwd, void* data) :
        ScaMin(scaMin),
        ScaMax(scaMax),
        PltMin(pltMin),
        PltMax(pltMax),
        PixMin(pixMin),
        M(m),
        TransformFwd(fwd),
        TransformData(data)
    { }

    template <typename T> float operator()(T p) const {
        if (TransformFwd != nullptr) {
            double s = TransformFwd(p, TransformData);
            double t = (s - ScaMin) / (ScaMax - ScaMin);
            p = PltMin + (PltMax - PltMin) * t;
        }
        return (float)(PixMin + M * (p - PltMin));
    }

    double ScaMin, ScaMax, PltMin, PltMax, PixMin, M;
    ImPlotTransform TransformFwd;
    void* TransformData;
};

struct Transformer2 {
    Transformer2(const ImPlotAxis& x_axis, const ImPlotAxis& y_axis) :
        Tx(x_axis.PixelMin,
            x_axis.Range.Min,
            x_axis.Range.Max,
            x_axis.ScaleToPixel,
            x_axis.ScaleMin,
            x_axis.ScaleMax,
            x_axis.TransformForward,
            x_axis.TransformData),
        Ty(y_axis.PixelMin,
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
        out.x = Tx(plt.x);
        out.y = Ty(plt.y);
        return out;
    }

    template <typename T> ImVec2 operator()(T x, T y) const {
        ImVec2 out;
        out.x = Tx(x);
        out.y = Ty(y);
        return out;
    }

    Transformer1 Tx;
    Transformer1 Ty;
};

struct RendererBase {
    RendererBase(int prims, int idx_consumed, int vtx_consumed) :
        Prims(prims),
        IdxConsumed(idx_consumed),
        VtxConsumed(vtx_consumed)
    { }
    const int Prims;
    Transformer2 Transformer;
    const int IdxConsumed;
    const int VtxConsumed;
};


template <class _Getter1, class _Getter2>
struct RendererBarsFillH : RendererBase {
    RendererBarsFillH(const _Getter1& getter1, const _Getter2& getter2, ImU32 col, double height) :
        RendererBase(ImMin(getter1.Count, getter1.Count), 6, 4),
        Getter1(getter1),
        Getter2(getter2),
        Col(col),
        HalfHeight(height / 2)
    {}
    void Init(ImDrawList& draw_list) const {
        UV = draw_list._Data->TexUvWhitePixel;
    }
    bool Render(ImDrawList& draw_list, const ImRect& cull_rect, int prim) const {
        ImPlotPoint p1 = Getter1(prim);
        ImPlotPoint p2 = Getter2(prim);
        p1.y += HalfHeight;
        p2.y -= HalfHeight;
        ImVec2 P1 = this->Transformer(p1);
        ImVec2 P2 = this->Transformer(p2);
        float height_px = ImAbs(P1.y - P2.y);
        if (height_px < 1.0f) {
            P1.y += P1.y > P2.y ? (1 - height_px) / 2 : (height_px - 1) / 2;
            P2.y += P2.y > P1.y ? (1 - height_px) / 2 : (height_px - 1) / 2;
        }
        ImVec2 PMin = ImMin(P1, P2);
        ImVec2 PMax = ImMax(P1, P2);
        if (!cull_rect.Overlaps(ImRect(PMin, PMax)))
            return false;
        PrimRectFill(draw_list, PMin, PMax, Col, UV);
        return true;
    }
    const _Getter1& Getter1;
    const _Getter2& Getter2;
    const ImU32 Col;
    const double HalfHeight;
    mutable ImVec2 UV;
};

template <class _Renderer>
void RenderPrimitivesEx(const _Renderer& renderer, ImDrawList& draw_list, const ImRect& cull_rect) {
    unsigned int prims = renderer.Prims;
    unsigned int prims_culled = 0;
    unsigned int idx = 0;
    renderer.Init(draw_list);
    while (prims) {
        // find how many can be reserved up to end of current draw command's limit
        unsigned int cnt = ImMin(prims, (MaxIdx<ImDrawIdx>::Value - draw_list._VtxCurrentIdx) / renderer.VtxConsumed);
        // make sure at least this many elements can be rendered to avoid situations where at the end of buffer this slow path is not taken all the time
        if (cnt >= ImMin(64u, prims)) {
            if (prims_culled >= cnt)
                prims_culled -= cnt; // reuse previous reservation
            else {
                // add more elements to previous reservation
                draw_list.PrimReserve((cnt - prims_culled) * renderer.IdxConsumed, (cnt - prims_culled) * renderer.VtxConsumed);
                prims_culled = 0;
            }
        }
        else
        {
            if (prims_culled > 0) {
                draw_list.PrimUnreserve(prims_culled * renderer.IdxConsumed, prims_culled * renderer.VtxConsumed);
                prims_culled = 0;
            }
            cnt = ImMin(prims, (MaxIdx<ImDrawIdx>::Value - 0/*draw_list._VtxCurrentIdx*/) / renderer.VtxConsumed);
            // reserve new draw command
            draw_list.PrimReserve(cnt * renderer.IdxConsumed, cnt * renderer.VtxConsumed);
        }
        prims -= cnt;
        for (unsigned int ie = idx + cnt; idx != ie; ++idx) {
            if (!renderer.Render(draw_list, cull_rect, idx))
                prims_culled++;
        }
    }
    if (prims_culled > 0)
        draw_list.PrimUnreserve(prims_culled * renderer.IdxConsumed, prims_culled * renderer.VtxConsumed);
}

template <template <class, class> class _Renderer, class _Getter1, class _Getter2, typename ...Args>
void RenderPrimitives2(const _Getter1& getter1, const _Getter2& getter2, Args... args) {
    ImDrawList& draw_list = *ImPlot::GetPlotDrawList();
    const ImRect& cull_rect = ImPlot::GetCurrentPlot()->PlotRect;
    RenderPrimitivesEx(_Renderer<_Getter1, _Getter2>(getter1, getter2, args...), draw_list, cull_rect);
}

void PrimRectFill(ImDrawList& draw_list, const ImVec2& Pmin, const ImVec2& Pmax, ImU32 col, const ImVec2& uv) {
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

template <typename Getter1, typename Getter2>
void PlotBarsStackEx(std::string label_id, const Getter1& getter1, const Getter2& getter2, double height, ImU32 bar_color,ImPlotBarsFlags flags) {
    if (ImPlot::BeginItemEx(label_id.c_str(), FitterBarH<Getter1, Getter2>(getter1, getter2, height), flags, ImPlotCol_Fill)) {
        if (getter1.Count <= 0 || getter2.Count <= 0) {
            EndItem();
            return;
        }
        const ImU32 col_fill = bar_color;
        RenderPrimitives2<RendererBarsFillH>(getter1, getter2, col_fill, height);
        EndItem();
    }
}

void Demo_BarPlots() {
    std::deque<uint64_t> times1 = { 0,5,11,18,26,35,45,56,68,81 };
    std::deque<bool> data1 = { true,false,true,false,true,false,true,false,true,false };
    std::deque<uint64_t> times2 = { 5,25 };
    std::deque<bool> data2 = { false,true,true,false };
    ImVec4 color_true = ImVec4(0, 0, 1, 1);
    ImVec4 color_false = ImVec4(1, 0, 0, 1);
    
    std::vector<double> default_data;
    default_data.reserve(times2.size());
    std::vector<double> x_data;
    x_data.reserve(times2.size()-1);
    std::vector<double> y_data;
    y_data.reserve(times2.size()-1);
    std::vector<double> bar_width;
    bar_width.reserve(times2.size() - 1);

    for (size_t i = 0; i < times2.size() - 1; ++i)
    {
        x_data.push_back(static_cast<double>(times2[i]) + (times2[i + 1] - times2[i]) / 2.0);
        //y_data.push_back(1);
        bar_width.push_back(times2[i + 1] - times2[i]);
        //ImPlot::PushStyleColor(ImPlotCol_Fill, data1[i] ? color_true :color_false);
    }

    for (size_t i = 0; i < times2.size() ; ++i)
    {
        default_data.push_back(static_cast<double>(times2[i]));
    }


    if (ImPlot::BeginPlot("Bar Plot"))
    {
        for (size_t i = 0; i < times2.size() - 1; ++i)
        {
            ImVec4 color = data1[i] ? color_true : color_false;
            ImPlot::PushStyleColor(ImPlotCol_Fill, color);
            //uint64_t x_data[2] = { times1[i] ,times1[i]};
            //uint64_t y_data[2] = { times1[i], times1[i + 1] - times1[i] };
            //ImPlot::PlotBars_Continous("State Bars", y_data, 1, 0.4, ImPlotBarsFlags_Horizontal);
            
            const char* text = data1[i] ? "TRUE" : "FALSE";
            ImPlot::PlotText(text, times1[i], 0.5f);
            ImPlot::PopStyleColor();
        }
        ImPlot::PlotBars("Horizontal2", default_data.data(), default_data.size(), 0.1, 0, ImPlotBarsFlags_Horizontal);
        ImPlot::EndPlot();
    }

}

//-----------------------------------------------------------------------------

void Demo_BarGroups() {
    get_color_based_on_value(true);
    std::deque<uint64_t>  times = { 0,10,15,16,19,21,23,25,29,30 };
    std::deque<bool> data1 = { true,false,true,false,true,false,true,false,true,false };
    std::vector<uint64_t> datas(times.size() - 1);
    std::vector<bool> data_values(times.size() - 1);
    for (size_t i = 0; i < times.size() - 1; ++i)
    {
        datas[i] = times[i + 1] - times[i];
        data_values[i] = data1[i];
    }

    std::deque<uint64_t>  times2 = { 0,3,10,18,19,24,30 };
    std::deque<bool> data2 = { false,true,true,false,true,false,true };    
    std::vector<uint64_t> datas2(times2.size() - 1);
    std::vector<bool> data_values2(times2.size() - 1);
    for (size_t i = 0; i < times2.size() - 1; ++i)
    {
        datas2[i] = times2[i + 1] - times2[i];
        data_values2[i] = data2[i];
    }
    static float size = 0.67f;

    static ImPlotBarGroupsFlags flags = 0;

    //ImGui::CheckboxFlags("Stacked", (unsigned int*)&flags, ImPlotBarGroupsFlags_Stacked);
    //ImGui::SameLine();


    if (ImPlot::BeginPlot("Bar Group", ImVec2(-1,0),ImPlotFlags_Equal)) {
        ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);

        ImPlot::SetupAxes("Time", "Topic", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxis(ImAxis_Y1, NULL,ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels);
        PlotBarStacks("testa", datas.data(), data_values,datas.size(), 0.1, 0, flags | ImPlotBarGroupsFlags_Horizontal | ImPlotBarGroupsFlags_Stacked);
        PlotBarStacks("testb", datas2.data(), data_values2,datas2.size(), 0.1, 0.2, flags | ImPlotBarGroupsFlags_Horizontal | ImPlotBarGroupsFlags_Stacked);

        for (size_t i = 0; i < data1.size(); ++i)
        {
            const char* text = data1[i] ? "TRUE" : "FALSE";
            ImPlot::PlotText(text, times[i], 0.1f);
        }
        for (size_t i = 0; i < data2.size(); ++i)
        {
            const char* text = data2[i] ? "TRUE" : "FALSE";
            ImPlot::PlotText(text, times2[i], 0.3f);
        }

        ImPlot::EndPlot();
    }
}

void Demo_BarGroups2() {
    static ImS8  data[30] = { 83, 67, 23, 89, 83, 78, 91, 82, 85, 90,  // midterm
                             80, 62, 56, 99, 55, 78, 88, 78, 90, 100, // final
                             80, 69, 52, 92, 72, 78, 75, 76, 89, 95 }; // course

    static const char* ilabels[] = { "Midterm Exam","Final Exam","Course Grade" };
    static const char* glabels[] = { "S1","S2","S3","S4","S5","S6","S7","S8","S9","S10" };
    static const double positions[] = { 0,1,2,3,4,5,6,7,8,9 };

    static int items = 3;
    static int groups = 10;
    static float size = 0.67f;

    static ImPlotBarGroupsFlags flags = 0;
    static bool horz = false;

    ImGui::CheckboxFlags("Stacked", (unsigned int*)&flags, ImPlotBarGroupsFlags_Stacked);
    ImGui::SameLine();
    ImGui::Checkbox("Horizontal", &horz);

    ImGui::SliderInt("Items", &items, 1, 3);
    ImGui::SliderFloat("Size", &size, 0, 1);

    if (ImPlot::BeginPlot("Bar Group")) {
        ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
        if (horz) {
            ImPlot::SetupAxes("Score", "Student", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisTicks(ImAxis_Y1, positions, groups, glabels);
            ImPlot::PlotBarGroups(ilabels, data, items, groups, size, 0, flags | ImPlotBarGroupsFlags_Horizontal);
        }
        else {
            ImPlot::SetupAxes("Student", "Score", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisTicks(ImAxis_X1, positions, groups, glabels);
            ImPlot::PlotBarGroups(ilabels, data, items, groups, size, 0, flags);
        }
        ImPlot::EndPlot();
    }
}

//-----------------------------------------------------------------------------

void Demo_BarStacks()
{
    ImVec4 color_true = ImVec4(0, 0, 1, 1);
    ImVec4 color_false = ImVec4(1, 0, 0, 1);
    std::deque<uint64_t> times1 = { 0,10,15,16,19,21,23,25,29,30 };
    std::deque<bool> data1 = { true,false,true,true,false,true,true,false,true,false };

    static ImPlotColormap bools_color_map = -1;
    std::vector<ImVec4> colors;
    colors.reserve(data1.size());
    std::vector<const char*> labels;
    labels.push_back("TESTTOPIC");

    if (bools_color_map == -1)
    {
        for (int i = 0; i < data1.size(); i++)
        {
            ImVec4 color = data1[i] ? color_true : color_false;
            colors.push_back(color);
        }
        bools_color_map = ImPlot::AddColormap("testcolor1u", colors.data(), colors.size());
    }

    std::vector<uint64_t> data_plot;
    data_plot.reserve(times1.size() - 1);
    for (size_t i = 0; i < times1.size() - 1; ++i)
    {
        data_plot.push_back(times1[i + 1] - times1[i]);
    }


    std::vector<const char*> topics;
    topics.reserve(1);

    ImPlot::PushColormap(bools_color_map);
    if (ImPlot::BeginPlot("Test", ImVec2(-1, 400), ImPlotFlags_NoMouseText)) {
        ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Outside | ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_AutoFit);
        //ImPlot::SetupAxisTicks(ImAxis_Y1, 0, 0, 1, labels.data(), false);

        ImPlot::PlotBarGroups(labels.data(), data_plot.data(), labels.size(), data_plot.size(), 0.75, 0, ImPlotBarGroupsFlags_Stacked | ImPlotBarGroupsFlags_Horizontal);
        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();
}


// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // 4. Test bar plots
        Demo_BarGroups();


        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

