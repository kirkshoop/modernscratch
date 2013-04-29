#pragma once

namespace rxtheme {

namespace rx=rxcpp;
namespace l=LIBRARIES_NAMESPACE;

template<class MessageArg>
class theme_data {
public:
    typedef MessageArg Message;

    ~theme_data() {
        themechanged.Dispose();}

    theme_data(HWND w, std::wstring c, const typename Message::Observable& m) :
            window(w),
            classId(std::move(c)),
            messages(m)
    {
        auto loadThemeData = [this](){
            handle.reset();
            handle.reset(OpenThemeData(window, classId.c_str()));
            if (!handle) {
                throw std::runtime_error("theme data not loaded");}
        };

        themechanged.Set(rx::from(messages)
            .where(rxmsg::messageId<rxmsg::wm::themechanged>())
            .subscribe([loadThemeData](const Message& m){
                rxmsg::set_handled(m); loadThemeData();}));

        loadThemeData();
    }

    HWND window;
    rx::SerialDisposable themechanged;
    std::wstring classId;
    typename Message::Observable messages;
    l::wr::unique_theme_data handle;

private:
};

template<class Tag, class MessageArg>
struct label {
    typedef Tag tag;
    typedef decltype(rx_measurement_traits(tag())) traits;
    typedef MessageArg Message;
    typedef typename traits::Rect Rect;
    typedef typename traits::Measurement Measurement;

    label(HWND w, const typename Message::Observable& m) : 
        window(w), messages(m), theme(std::make_shared<theme_data<Message>>(window, L"static", messages)) {
    } 
    HWND window;
    typename Message::Observable messages;
    std::shared_ptr<theme_data<Message>> theme;
    Measurement measureText() 
    {
        Rect r;
        Measurement m;
        HDC dc = GetDC(window);
        auto text = l::wnd::get_text(window);
        GetThemeTextExtent(
          theme->handle.get(),
          dc,
          TEXT_LABEL,
          0,
          text.c_str(),
          text.size(),
          DT_LEFT | DT_TOP,
          nullptr,
          &r.r
        );
        m.left = r.getLeft();
        m.top = r.getTop();
        m.right = r.getRight();
        m.bottom = r.getBottom();
        return m;
    }
};

}