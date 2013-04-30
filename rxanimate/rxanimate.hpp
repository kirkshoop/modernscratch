
namespace rxanim {
namespace rx=rxcpp;

    template<class Clock>
    class time_range {
    public:
        typedef Clock clock;
        typedef typename clock::time_point time_point;
        typedef typename clock::duration duration_type;

        time_range() {}
        time_range(time_point s, time_point f) : start(s), finish(f) {}
        time_range(time_point s, duration_type d) : start(s), finish(s + d) {}
        time_point start;
        time_point finish;
        bool empty() {return start == finish;}
        bool contains(time_point p) {return p >= start && p < finish;}
        duration_type duration() {return finish - start;}
    };


    float easeNone(float t) {return t;}
    float easeSquare(float t) {return t*t;}
    float easeSquareRoot(float t) {return sqrt(t);}
    template<int Num, int Den = 1>
    float easePow(float t) {return pow(t, Num/Den);}
    template<int Den>
    float easeRoot(float t) {return pow(t, 1/Den);}
    float easeElasticDamp(float t) {
        return 1.0f - pow(2, -10 * t) * sin(4 * 3.14 * (t+3.14));}
    float easeElasticAmp(float t) {
        return pow(2, 10 * (t - 1)) * sin(4 * 3.14 * (t+3.14));}
    float easeBounceDamp(float t) {
        if (t < .27) {
            return (4*3.14*t*t);}
        else if (t < .71) {
            return (4*3.14*(t-.5)*(t-.5))+.4;}
        else if (t < .89) {
            return (4*3.14*(t-.8)*(t-.8))+.92;}
        else {
            return (4*3.14*(t-.94)*(t-.94))+.97;}}
    float easeBounceAmp(float t) {
        return 1.0f - easeBounceDamp(t);}

    struct animation_state {
        enum type {Invalid, Pending, Running, Finished};
    };

    template<class TimeRange, class TimePoint>
    animation_state::type runOnce(TimeRange r, TimePoint p) {
        if (p < r.start) {
            return animation_state::Pending;}
        if (r.empty() || !r.contains(p)) {
            return animation_state::Finished;}
        return animation_state::Running;
    }

    template<size_t N, class TimeRange, class TimePoint>
    animation_state::type runN(TimeRange r, TimePoint p) {
        TimeRange full = r;
        full.finish = r.start + (r.duration() * N);
        return runOnce(full, p);
    }

    template<class TimeRange, class TimePoint>
    float adjustNone(TimeRange r, TimePoint p) {
        size_t den = r.duration().count();
        size_t num = (p - r.start).count();
#if 0
        std::wstringstream logmsg;
        std::time_t tt = std::chrono::system_clock::to_time_t(p);
        logmsg << L"adjust  time: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(r.start);
        logmsg << L"       start: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(r.finish);
        logmsg << L"      finish: " << std::ctime(&tt); 
        logmsg << L"   numerator: " << num << std::endl; 
        logmsg << L" denominator: " << den << std::endl; 
        logmsg << L"       place: " << (num%den) << std::endl; 
        logmsg << L"   iteration: " << (num/den) << std::endl; 
        logmsg << L"  normalized: " << (static_cast<float>(num%den)/den) << std::endl; 
        OutputDebugString(logmsg.str().c_str());
#endif
        return static_cast<float>(num%den) / den;}

    template<class TimeRange, class TimePoint>
    float adjustReverse(TimeRange r, TimePoint p) {
        return 1.0f - adjustNone(r, p);}

    template<class TimeRange, class TimePoint>
    float adjustPingPong(TimeRange r, TimePoint p) {
        size_t den = r.duration().count();
        size_t num = (p - r.start).count();
        float none = adjustNone(r, p);
#if 0
        std::wstringstream logmsg;
        std::time_t tt = std::chrono::system_clock::to_time_t(p);
        logmsg << L"pingpong time: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(r.start);
        logmsg << L"        start: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(r.finish);
        logmsg << L"       finish: " << std::ctime(&tt);
        logmsg << L"    numerator: " << num << std::endl; 
        logmsg << L"  denominator: " << den << std::endl; 
        logmsg << L"        place: " << (num%den) << std::endl; 
        logmsg << L"    iteration: " << (num/den) << std::endl; 
        logmsg << L"   normalized: " << (static_cast<float>(num%den)/den) << std::endl; 
        OutputDebugString(logmsg.str().c_str());
#endif
        return 0==(static_cast<size_t>(num/den) % 2) ? none : 1.0f - none;}

    // used to implement bouncing etc..
    // in, is a percentage that represents the point in the time_range 
    typedef std::function<float (float)> ease_type;
    // called to execute the animation step at time_point as if it was at normalized time factor float
    typedef std::function<void (animation_state::type, float)> step_type;

    template<class Clock>
    class time_animation {
    public:
        typedef Clock clock;
        typedef typename clock::duration duration_type;
        typedef typename clock::time_point time_point;
        typedef time_range<clock> time_range;

        // used to constrain the run time of the animation
        typedef std::function<animation_state::type (time_range, time_point)> state_type;
        // used to implement ping/pong, repeat, etc..
        // returns a float from 0-1 that represents where in time_range  
        // the passed in time_point (may not be in time_range) represents.
        typedef std::function<float (time_range, time_point)> adjust_type;
        typedef ease_type ease_type;
        typedef step_type step_type;

        time_animation() : 
            state(runOnce<time_range, time_point>), adjust(adjustNone<time_range, time_point>), ease(easeNone)
            {}
        explicit time_animation(state_type st) : 
            state(std::move(st)), adjust(adjustNone<time_range, time_point>), ease(easeNone) 
            {}
        time_animation(state_type st, ease_type ease) : 
            state(std::move(st)), adjust(adjustNone<time_range, time_point>), ease(std::move(ease)) 
            {}
        time_animation(state_type st, adjust_type a) : 
            state(std::move(st)), adjust(std::move(a)), ease(easeNone)
            {}
        time_animation(state_type st, adjust_type a, ease_type e) : 
            state(std::move(st)), adjust(std::move(a)), ease(std::move(e)) 
            {}

        state_type state;
        adjust_type adjust;
        ease_type ease;
    };

    template<class Clock>
    animation_state::type step(time_animation<Clock>& ta, time_range<Clock> sc, time_range<Clock> sc_st, step_type& st, typename Clock::time_point t) {
        auto state = ta.state(sc, t);
        if (state == animation_state::Pending) {
            return state;}
        float time = 0.0f;
        if (state == animation_state::Running) {
            auto sc_time = ta.ease(ta.adjust(sc, t));
            auto sc_timepoint = sc.start + std::chrono::duration_cast<std::chrono::milliseconds>(sc.duration() * sc_time);
            if (!sc_st.contains(sc_timepoint)) {return animation_state::Pending;}
            time = (sc_timepoint - sc_st.start).count() / (sc_st.finish - sc_st.start).count();}
        else if (state == animation_state::Finished) {
            time = 1.0f;}
        st(state, time);
#if 0
        size_t den = sc.duration().count();
        size_t num = (t - sc.start).count();
        std::wstringstream logmsg;
        std::time_t tt = std::chrono::system_clock::to_time_t(t);
        logmsg << L"step      time: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(sc.start);
        logmsg << L"         start: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(sc.finish);
        logmsg << L"        finish: " << std::ctime(&tt); 
        logmsg << L"         state: " << (state == animation_state::Pending ? "Pending" : 
                                       (state == animation_state::Running ? "Running" : 
                                       (state == animation_state::Finished ? "Finished" : "Unknown"))) << std::endl; 
        logmsg << L"     numerator: " << num << std::endl; 
        logmsg << L"   denominator: " << den << std::endl; 
        if (den != 0) {
            logmsg << L"         place: " << (num%den) << std::endl; 
            logmsg << L"     iteration: " << (num/den) << std::endl; 
            logmsg << L"    normalized: " << (static_cast<float>(num%den)/den) << std::endl;}
        logmsg << L"        result: " << time << std::endl; 
        OutputDebugString(logmsg.str().c_str());
#endif
        return state;
    }

    template<class Clock>
    animation_state::type step(time_animation<Clock>& ta, time_range<Clock> sc, time_range<Clock> sc_st, step_type& st) {
        auto t = Clock::now();
        if (!sc_st.contains(t)) {return animation_state::Pending;}
        auto state = ta.state(sc, t);
        if (state == animation_state::Pending) {
            return state;}
        float time = 0.0f;
        if (state == animation_state::Running) {
            auto sc_time = ta.ease(ta.adjust(sc, t));
            auto sc_timepoint = sc.start + std::chrono::duration_cast<std::chrono::milliseconds>(sc.duration() * sc_time);
            if (!sc_st.contains(sc_timepoint)) {return animation_state::Pending;}
            time = (sc_timepoint - sc_st.start).count() / (sc_st.finish - sc_st.start).count();}
        else if (state == animation_state::Finished) {
            time = 1.0f;}
        st(state, time);
#if 0
        size_t den = sc.duration().count();
        size_t num = (t - sc.start).count();
        std::wstringstream logmsg;
        std::time_t tt = std::chrono::system_clock::to_time_t(t);
        logmsg << L"step(now) time: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(sc.start);
        logmsg << L"         start: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(sc.finish);
        logmsg << L"        finish: " << std::ctime(&tt); 
        logmsg << L"         state: " << (state == animation_state::Pending ? "Pending" : 
                                       (state == animation_state::Running ? "Running" : 
                                       (state == animation_state::Finished ? "Finished" : "Unknown"))) << std::endl; 
        logmsg << L"     numerator: " << num << std::endl; 
        logmsg << L"   denominator: " << den << std::endl; 
        if (den != 0) {
            logmsg << L"         place: " << (num%den) << std::endl; 
            logmsg << L"     iteration: " << (num/den) << std::endl; 
            logmsg << L"    normalized: " << (static_cast<float>(num%den)/den) << std::endl;}
        logmsg << L"        result: " << time << std::endl; 
        OutputDebugString(logmsg.str().c_str());
#endif
        return state;
    }

    template<class T>
    struct lerp_value {
        lerp_value(std::shared_ptr<rx::Observer<T>> o, T i, T f) : initial(std::move(i)), final(std::move(f)), observer(std::move(o)) {}
        T initial;
        T final;
        std::shared_ptr<rx::Observer<T>> observer;
        void operator()(animation_state::type state, float time) {
            observer->OnNext(initial + ((final - initial) * time));
            if (state == animation_state::Finished) {
                observer->OnCompleted();}
        }
    };

    template<class... Value>
    struct lerp_value<std::tuple<Value...>> {
        typedef std::tuple<Value...> T;
        lerp_value(std::shared_ptr<rx::Observer<T>> o, T i, T f) : initial(std::move(i)), final(std::move(f)), observer(std::move(o)) {}
        lerp_value(std::shared_ptr<rx::Observer<T>> o, Value... i, Value... f)
            : inital(std::make_tuple(std::move(i)...)), final(std::make_tuple(std::move(f)...)), observer(std::move(o)) {}
        T initial;
        T final;
        std::shared_ptr<rx::Observer<T>> observer;
        void operator()(animation_state::type state, float time) {
            rx::DispatchTuple(std::tuple_cat(initial, final), [time, this](Value... initialv, Value... finalv) {
                T result(static_cast<Value>(initialv + ((finalv - initialv) * time))...);
#if 0
                std::wstringstream logmsg;
                logmsg << L"step - initial: " << tuple_insert(initial) << std::endl; 
                logmsg << L"step -   final: " << tuple_insert(final) << std::endl; 
                logmsg << L"step -  result: " << tuple_insert(result) << std::endl; 
                OutputDebugString(logmsg.str().c_str());
#endif
                observer->OnNext(std::move(result));
            });
        }
    };

    template<class T, class InitialSelector, class TimeSelector>
    auto Animate(
        const std::shared_ptr<rx::Observable<T>>& sourceFinal, 
        const std::shared_ptr<rx::Observable<typename rx::Scheduler::clock::time_point>>& sourceInterval, 
        time_animation<typename rx::Scheduler::clock> ta,
        InitialSelector initialSelector,
        TimeSelector timeSelector 
        ) 
        -> std::shared_ptr<rx::Observable<T>> {
        typedef typename rx::Scheduler::clock clock;
        typedef time_animation<clock> time_animation;
        typedef typename time_animation::time_range time_range;
        typedef typename time_animation::time_point time_point;
        typedef typename time_animation::duration_type time_duration;
        typedef typename time_animation::step_type step_type;
        typedef std::pair<time_range, lerp_value<T>> lerp_value_type;
        typedef std::vector<std::shared_ptr<lerp_value_type>> lerps_type;
        typedef typename lerps_type::difference_type difference_type;
        return rx::CreateObservable<T>(
        // subscribe
            [=](const std::shared_ptr<rx::Observer<T>>& observer) 
                -> rx::Disposable {
                struct State {
                    State(time_animation ta) : 
                        running(false), animation(std::move(ta)) {}
                    std::mutex lock;
                    lerps_type lerps;
                    bool running;
                    time_animation animation;
                };
                auto state = std::make_shared<State>(ta);
                rx::ComposableDisposable cd;

                auto wd = cd.Add(rx::Disposable::Empty());

                cd.Add(rx::from(sourceFinal)
                    .subscribe(
                    // on next
                        [=](T final) {
                            auto now = clock::now();

                            auto start = now;
                            auto initial = final;

                            {
                                std::unique_lock<std::mutex> guard(state->lock);

                                if (!state->lerps.empty()) {
                                    start = state->lerps.back()->first.finish;
                                    initial = state->lerps.back()->second.final;}
                            }

                            initial = initialSelector(initial);

                            auto scope = timeSelector(now, start, initial, final);

                            bool needInterval = false;
                            {
                                std::unique_lock<std::mutex> guard(state->lock);

                                state->lerps.push_back(std::make_shared<lerp_value_type>(
                                    scope, 
                                    lerp_value<T>(observer, initial, final)));

                                if (!state->running) {
                                    state->running = true; needInterval = true;}
                            }

                            if (needInterval) {
                                auto intervalDisposable = rx::from(sourceInterval)
                                    .subscribe(
                                    // on next
                                        [=](time_point thisTick) {
    #if 0
                                            {std::wstringstream logmsg;
                                            std::time_t tt = std::chrono::system_clock::to_time_t(thisTick);
                                            logmsg << L"Tick: " << std::ctime(&tt) << std::endl; 
                                            OutputDebugString(logmsg.str().c_str());}
    #endif
                                            lerps_type lerps;
                                            {
                                                std::unique_lock<std::mutex> guard(state->lock);
                                                lerps = state->lerps;
                                            }

                                            if (!lerps.empty()) {
                                                auto begin = lerps.begin();
                                                auto cursor = lerps.begin();
                                                auto end = lerps.end();
                                                auto sc = lerps.front()->first;
                                                sc.finish = lerps.back()->first.finish;
                                                auto sc_state = ta.state(sc, thisTick);
                                                if (sc_state == animation_state::Running) {
                                                    auto sc = lerps.front()->first;
                                                    sc.finish = lerps.back()->first.finish;

                                                    auto st_time = state->animation.ease(state->animation.adjust(sc, thisTick));

                                                    auto st_timepoint = sc.start + std::chrono::duration_cast<std::chrono::milliseconds>(sc.duration() * st_time);
                                                    for (;cursor != end && !(*cursor)->first.contains(st_timepoint); ++cursor);

                                                    if (cursor != end) {
                                                        auto& lerp = *cursor;
                                                        auto time = static_cast<float>((st_timepoint - lerp->first.start).count()) / (lerp->first.finish - lerp->first.start).count();
    #if 0
                                                        size_t num = (st_timepoint - lerp->first.start).count();
                                                        size_t den = (lerp->first.finish - lerp->first.start).count();
                                                        std::wstringstream logmsg;
                                                        logmsg <<     L"tick      time: " << time_insert(thisTick) << std::endl; 
                                                        logmsg <<     L"    eased time: " << time_insert(st_timepoint) << std::endl; 
                                                        logmsg <<     L"         scope: " << time_insert(sc.start) << L"-" << time_insert(sc.finish) << std::endl; 
                                                        logmsg <<     L"    step scope: " << time_insert(lerp->first.start) << L"-" << time_insert(lerp->first.finish) << std::endl; 
                                                        logmsg <<     L"     numerator: " << num << std::endl; 
                                                        logmsg <<     L"   denominator: " << den << std::endl; 
                                                        if (den != 0) {
                                                            logmsg << L"         place: " << (num%den) << std::endl; 
                                                            logmsg << L"     iteration: " << (num/den) << std::endl; 
                                                            logmsg << L"    normalized: " << (static_cast<float>(num%den)/den) << std::endl;}
                                                        logmsg <<     L"        result: " << time << std::endl << std::endl; 
                                                        OutputDebugString(logmsg.str().c_str());
    #endif
                                                        lerp->second(sc_state, time);}}
                                                else if (sc_state == animation_state::Finished) {
                                                    lerps.back()->second(sc_state, 1.0f);}

                                                {
                                                    std::unique_lock<std::mutex> guard(state->lock);
                                                    if (sc_state == animation_state::Finished) {
                                                        state->lerps.clear();
                                                        state->running = false;
                                                        auto strong = wd.lock();
                                                        guard.unlock();
                                                        strong->Dispose();}
                                                }
                                            }
                                        },
                                    // on completed
                                        [=](){
                                            observer->OnCompleted();
                                            cd.Dispose();
                                        }, 
                                    // on error
                                        [=](const std::exception_ptr& e){
                                            observer->OnError(e);
                                            cd.Dispose();
                                    });
                                {
                                    std::unique_lock<std::mutex> guard(state->lock);
                                    auto strong = wd.lock();
                                    if (strong) {
                                        *strong.get() = std::move(intervalDisposable);}
                                }
                            }
                        },
                    // on completed
                        [=](){
                            observer->OnCompleted();
                            cd.Dispose();
                        }, 
                    // on error
                        [=](const std::exception_ptr& e){
                            observer->OnError(e);
                            cd.Dispose();
                    }));
                return cd;
            });
    }

    struct animate {};
    template<class T, class InitialSelector, class TimeSelector>
    auto rxcpp_chain(animate&&, 
        const std::shared_ptr<rx::Observable<T>>& sourceFinal, 
        const std::shared_ptr<rx::Observable<typename rx::Scheduler::clock::time_point>>& sourceInterval, 
        time_animation<typename rx::Scheduler::clock> ta,
        InitialSelector initialSelector,
        TimeSelector timeSelector 
        ) 
        -> decltype(Animate(sourceFinal, sourceInterval, ta, initialSelector, timeSelector)) {
        return      Animate(sourceFinal, sourceInterval, ta, initialSelector, timeSelector);
    }
}
using rxanim::rxcpp_chain;
