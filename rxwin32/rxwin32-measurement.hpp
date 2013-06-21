#pragma once

namespace rxmeasurement {

namespace rx=rxcpp;

namespace detail {


    template<class Tag>
    struct operators {
        typedef Tag tag;
        typedef decltype(rx_measurement_traits(tag())) traits;
        typedef typename traits::Message Message;
        typedef typename traits::XPoint XPoint;
        typedef typename traits::YPoint YPoint;
        typedef typename traits::XExtent XExtent;
        typedef typename traits::YExtent YExtent;
        typedef typename traits::Point Point;
        typedef typename traits::Extent Extent;
        typedef typename traits::Rect Rect;
        typedef typename traits::Measurement Measurement;

        template<class Message>
        static auto SelectClientMeasurement(const std::shared_ptr<rx::Observable<Message>>& source) 
            -> std::shared_ptr<rx::Observable<Measurement>> {
            return rx::CreateObservable<Measurement>(
                [=](const std::shared_ptr<rx::Observer<Measurement>>& observer) {
                    return rx::from(source)
                        .where(rxmsg::messageId<rxmsg::wm::windowposchanged>())
                        .select([](const Message& msg){
                            return rxmsg::crack_message<rxmsg::wm::windowposchanged>(msg);})
                        .subscribe(
                        // on next
                            rxcpp::MakeTupleDispatch([=](const LPWINDOWPOS , const Message& msg){
                                RECT client = {};
                                GetClientRect(msg.window, &client);
                                Measurement m(
                                    XPoint(client.left),
                                    YPoint(client.top),
                                    XPoint(client.right),
                                    YPoint(client.bottom)
                                );
                                observer->OnNext(m);
                            }),
                        // on completed
                            [=](){
                                observer->OnCompleted();
                            }, 
                        // on error
                            [=](const std::exception_ptr& e){
                                observer->OnError(e);
                        });
                });
        }

        template<class Message>
        static auto SelectScreenMeasurement(const std::shared_ptr<rx::Observable<Message>>& source) 
            -> std::shared_ptr<rx::Observable<Measurement>> {
            return rx::CreateObservable<Measurement>(
                [=](const std::shared_ptr<rx::Observer<Measurement>>& observer) {
                    return rx::from(source)
                        .where(rxmsg::messageId<rxmsg::wm::windowposchanged>())
                        .select([](const Message& msg){
                            return rxmsg::crack_message<rxmsg::wm::windowposchanged>(msg);})
                        .subscribe(
                        // on next
                            rxcpp::MakeTupleDispatch([=](const LPWINDOWPOS pos, const Message& msg){
                                RECT screen = {};
                                GetWindowRect(msg.window, &screen);
                                Measurement m(
                                    XPoint(screen.left),
                                    YPoint(screen.top),
                                    XPoint(screen.right),
                                    YPoint(screen.bottom)
                                );
                                observer->OnNext(m);
                            }),
                        // on completed
                            [=](){
                                observer->OnCompleted();
                            }, 
                        // on error
                            [=](const std::exception_ptr& e){
                                observer->OnError(e);
                        });
                });
        }

        template<class Message>
        static auto SelectParentMeasurement(const std::shared_ptr<rx::Observable<Message>>& source) 
            -> std::shared_ptr<rx::Observable<Measurement>> {
            return rx::CreateObservable<Measurement>(
                [=](const std::shared_ptr<rx::Observer<Measurement>>& observer) {
                    return rx::from(source)
                        .where(rxmsg::messageId<rxmsg::wm::windowposchanged>())
                        .select([](const Message& msg){
                            return rxmsg::crack_message<rxmsg::wm::windowposchanged>(msg);})
                        .subscribe(
                        // on next
                            rxcpp::MakeTupleDispatch([=](const LPWINDOWPOS , const Message& msg){
                                RECT screen = {};
                                GetWindowRect(msg.window, &screen);
                                auto parent = GetAncestor(msg.window, GA_PARENT);
                                RECT mapped = screen;
                                if (parent != HWND_DESKTOP) {
                                    MapWindowPoints(HWND_DESKTOP, parent, (LPPOINT)&mapped, 2);
                                }
                                Measurement m(
                                    XPoint(mapped.left),
                                    YPoint(mapped.top),
                                    XPoint(mapped.right),
                                    YPoint(mapped.bottom)
                                );
                                observer->OnNext(m);
                            }),
                        // on completed
                            [=](){
                                observer->OnCompleted();
                            }, 
                        // on error
                            [=](const std::exception_ptr& e){
                                observer->OnError(e);
                        });
                });
        }

    };

}

template<class Tag>
struct select_client_measurement {};
template<class Tag, class Message>
auto rxcpp_chain(select_client_measurement<Tag>&&, const std::shared_ptr<rx::Observable<Message>>& source) 
    -> decltype(detail::operators<Tag>::SelectClientMeasurement(source)) {
    return      detail::operators<Tag>::SelectClientMeasurement(source);
}

template<class Tag>
struct select_screen_measurement {};
template<class Tag, class Message>
auto rxcpp_chain(select_screen_measurement<Tag>&&, const std::shared_ptr<rx::Observable<Message>>& source) 
    -> decltype(detail::operators<Tag>::SelectScreenMeasurement(source)) {
    return      detail::operators<Tag>::SelectScreenMeasurement(source);
}

template<class Tag>
struct select_parent_measurement {};
template<class Tag, class Message>
auto rxcpp_chain(select_parent_measurement<Tag>&&, const std::shared_ptr<rx::Observable<Message>>& source) 
    -> decltype(detail::operators<Tag>::SelectParentMeasurement(source)) {
    return      detail::operators<Tag>::SelectParentMeasurement(source);
}

template<class Tag>
struct window_measure {
    typedef Tag tag;
    typedef decltype(rx_measurement_traits(tag())) traits;
    typedef typename traits::XPoint XPoint;
    typedef typename traits::YPoint YPoint;
    typedef typename traits::XExtent XExtent;
    typedef typename traits::YExtent YExtent;
    typedef typename traits::Point Point;
    typedef typename traits::Extent Extent;
    typedef typename traits::Rect Rect;
    typedef typename traits::Measurement Measurement;

    typedef select_client_measurement<Tag> select_client_measurement;
    typedef select_screen_measurement<Tag> select_screen_measurement;
    typedef select_parent_measurement<Tag> select_parent_measurement;
};

template<class Tag>
struct Measurement {
    typedef Tag tag;
    typedef decltype(rx_measurement_traits(tag())) traits;
    typedef typename traits::XPoint XPoint;
    typedef typename traits::YPoint YPoint;
    typedef typename traits::XExtent XExtent;
    typedef typename traits::YExtent YExtent;
    typedef typename traits::Point Point;
    typedef typename traits::Extent Extent;
    typedef typename traits::Rect Rect;

    Measurement() {}
#if 0
    explicit Measurement(Rect r) : 
        left(XPoint(r.r.left)),
        top(YPoint(r.r.top)),
        right(XPoint(r.r.right)),
        bottom(YPoint(r.r.bottom)) {}
#endif
    Measurement(
        XPoint l, YPoint t,
        XPoint r, YPoint b) : 
        left(std::move(l)), top(std::move(t)),
        right(std::move(r)), bottom(std::move(b)) {}
    XPoint left;
    YPoint top;
    XPoint right;
    YPoint bottom;
    XExtent width() const {return right - left;}
    YExtent height() const {return bottom - top;}
    XExtent width(float factor) const {return (right - left) * factor;}
    YExtent height(float factor) const {return (bottom - top) * factor;}
    Point origin() const {return Point(left, top);}
    Extent extent() const {return Extent(width(), height());}
    XPoint center() const {return left + (width() / 2);}
    YPoint middle() const {return top + (height() / 2);}
};

template<class Tag>
bool operator==(const Measurement<Tag>& l, const Measurement<Tag>& r) {
    return l.left == r.left && l.top == r.top && l.right == r.right && l.bottom == r.bottom;
}

}
using rxmeasurement::rxcpp_chain;

namespace rxmeasurement{namespace traits{
    template<class XPointArg>
    struct XPoint {
        XPointArg c;
        XPoint() {c = 0;}
        explicit XPoint(XPointArg carg) {c = carg;}
        XPoint(const XPoint& carg) : c(carg.c) {}
    };
    template<class YPointArg>
    struct YPoint {
        YPointArg c;
        YPoint() {c = 0;}
        explicit YPoint(YPointArg carg) {c = carg;}
        YPoint(const YPoint& carg) : c(carg.c) {}
    };
    template<class XExtentArg>
    struct XExtent {
        XExtentArg c;
        XExtent() {c = 0;}
        explicit XExtent(XExtentArg carg) {c = carg;}
        XExtent(const XExtent& carg) : c(carg.c) {}
    };
    template<class YExtentArg>
    struct YExtent {
        YExtentArg c;
        YExtent() {c = 0;}
        explicit YExtent(YExtentArg carg) {c = carg;}
        YExtent(const YExtent& carg) : c(carg.c) {}
    };
    template<class XPointArg, class YPointArg, class PointArg>
    struct Point {
        typedef XPoint<XPointArg> XPoint;
        typedef YPoint<YPointArg> YPoint;
        PointArg p;
        Point() {p.x = 0; p.y = 0;}
        Point(XPoint x, YPoint y) {p.x = x.c; p.y = y.c;}
        explicit Point(PointArg parg) {p = parg;}
        Point(const Point& p) : p(p.p) {}
        XPoint getX() {return XPoint(p.x);}
        YPoint getY() {return YPoint(p.y);}
    };
    template<class XExtentArg, class YExtentArg, class ExtentArg>
    struct Extent {
        typedef XExtent<XExtentArg> XExtent;
        typedef YExtent<YExtentArg> YExtent;
        ExtentArg e;
        Extent() {e.cx = 0; e.cy = 0;}
        Extent(XExtent cx, YExtent cy) {e.cx = cx.c; e.cy = cy.c;}
        explicit Extent(ExtentArg earg) {e = earg;}
        Extent(const Extent& e) : e(e.e) {}
        XExtent getCX() {return XExtent(e.cx);}
        YExtent getCY() {return YExtent(e.cy);}
    };
    template<class XPointArg, class YPointArg, class RectArg>
    struct Rect {
        typedef XPoint<XPointArg> XPoint;
        typedef YPoint<YPointArg> YPoint;
        RectArg r;
        Rect() {r.left = 0; r.top = 0; r.right = 0; r.bottom = 0;}
        Rect(XPoint l, YPoint t, XPoint r, YPoint b) {
            r.left = l.c; r.top = t.c; r.right = r.c; r.bottom = b.c;}
        explicit Rect(RectArg rarg) {r = rarg;}
        Rect(const Rect& p) : p(p.p) {}
        XPoint getLeft() {return XPoint(r.left);}
        YPoint getTop() {return YPoint(r.top);}
        XPoint getRight() {return XPoint(r.right);}
        YPoint getBottom() {return YPoint(r.bottom);}
    };

    template<class XPointArg>
    auto operator-(XPoint<XPointArg> r, XPoint<XPointArg> l) -> XExtent<decltype(r.c - l.c)> {
        return XExtent<decltype(r.c - l.c)>(r.c - l.c);}
    template<class XPointArg>
    bool operator<(XPoint<XPointArg> r, XPoint<XPointArg> l) {
        return r.c < l.c;}
    template<class XPointArg>
    bool operator==(XPoint<XPointArg> r, XPoint<XPointArg> l) {
        return r.c == l.c;}

    template<class YPointArg>
    auto operator-(YPoint<YPointArg> b, YPoint<YPointArg> t) -> YExtent<decltype(b.c - t.c)> {
        return YExtent<decltype(b.c - t.c)>(b.c - t.c);}
    template<class YPointArg>
    bool operator<(YPoint<YPointArg> b, YPoint<YPointArg> t) {
        return b.c < t.c;}
    template<class YPointArg>
    bool operator==(YPoint<YPointArg> b, YPoint<YPointArg> t) {
        return b.c == t.c;}

    template<class XPointArg, class XExtentArg>
    auto operator-(XPoint<XPointArg> c, XExtent<XExtentArg> e) -> XPoint<XPointArg> {
        return XPoint<XPointArg>(c.c - e.c);}
    template<class XPointArg, class XExtentArg>
    auto operator+(XPoint<XPointArg> c, XExtent<XExtentArg> e) -> XPoint<XPointArg> {
        return XPoint<XPointArg>(c.c + e.c);}

    template<class YPointArg, class YExtentArg>
    auto operator-(YPoint<YPointArg> c, YExtent<YExtentArg> e) -> YPoint<YPointArg> {
        return YPoint<YPointArg>(c.c - e.c);}
    template<class YPointArg, class YExtentArg>
    auto operator+(YPoint<YPointArg> c, YExtent<YExtentArg> e) -> YPoint<YPointArg> {
        return YPoint<YPointArg>(c.c + e.c);}

    template<class XExtentArg>
    bool operator<(XExtent<XExtentArg> l, XExtent<XExtentArg> r) {
        return l.c < r.c;}
    template<class XExtentArg>
    bool operator==(XExtent<XExtentArg> l, XExtent<XExtentArg> r) {
        return l.c == r.c;}

    template<class YExtentArg>
    bool operator<(YExtent<YExtentArg> l, YExtent<YExtentArg> r) {
        return l.c < r.c;}
    template<class YExtentArg>
    bool operator==(YExtent<YExtentArg> l, YExtent<YExtentArg> r) {
        return l.c == r.c;}

    template<class XExtentArg>
    auto operator/(XExtent<XExtentArg> e, XExtentArg d) -> XExtent<XExtentArg> {
        return XExtent<XExtentArg>(e.c/d);}
    template<class XExtentArg>
    auto operator*(XExtent<XExtentArg> e, XExtentArg d) -> XExtent<XExtentArg> {
        return XExtent<XExtentArg>(e.c*d);}

    template<class YExtentArg>
    auto operator/(YExtent<YExtentArg> e, YExtentArg d) -> YExtent<YExtentArg> {
        return YExtent<YExtentArg>(e.c/d);}
    template<class YExtentArg>
    auto operator*(YExtent<YExtentArg> e, YExtentArg d) -> YExtent<YExtentArg> {
        return YExtent<YExtentArg>(e.c*d);}

    template<class XPointArg, class YPointArg, class PointArg>
    bool operator<(Point<XPointArg, YPointArg, PointArg> l, Point<XPointArg, YPointArg, PointArg> r) {
        return l.p.x < r.p.x || l.p.y < r.p.y;}
    template<class XPointArg, class YPointArg, class PointArg>
    bool operator==(Point<XPointArg, YPointArg, PointArg> l, Point<XPointArg, YPointArg, PointArg> r) {
        return l.p.x == r.p.x || l.p.y == r.p.y;}

    template<class Tag, class XPointArg, class YPointArg, class XExtentArg, class YExtentArg, class PointArg, class ExtentArg, class RectArg>
    struct measurement_traits_builder {
        typedef Tag tag;
        typedef XPointArg XPointRaw;
        typedef YPointArg YPointRaw;
        typedef XExtentArg XExtentRaw;
        typedef YExtentArg YExtentRaw;
        typedef PointArg PointRaw;
        typedef ExtentArg ExtentRaw;
        typedef RectArg RectRaw;
        typedef XPoint<XPointArg> XPoint;
        typedef YPoint<YPointArg> YPoint;
        typedef XExtent<XExtentArg> XExtent;
        typedef YExtent<YExtentArg> YExtent;
        typedef Point<XPointArg, YPointArg, PointArg> Point;
        typedef Extent<XExtentArg, YExtentArg, ExtentArg> Extent;
        typedef Rect<XPointArg, YPointArg, RectArg> Rect;
        typedef Measurement<tag> Measurement;
    };

    struct Default {};
    measurement_traits_builder<Default, int, int, int, int, POINT, SIZE, RECT> rx_measurement_traits(Default&&);
}}
using rxmeasurement::traits::rx_measurement_traits;
