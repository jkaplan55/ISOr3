/// @file
///	@ingroup 	minapi
///	@copyright	Copyright (c) 2016, Cycling '74
///	@license	Usage of this file and its contents is governed by the MIT License

#pragma once


namespace c74::min {
   
    const long defaultuiflags = 0
         | JBOX_DRAWFIRSTIN		// 0
       // | JBOX_NODRAWBOX		// 1
        | JBOX_DRAWINLAST		// 2
        //	| JBOX_TRANSPARENT		// 3
        // 	| JBOX_NOGROW			// 4
        	| JBOX_GROWY			// 5
        //| JBOX_GROWBOTH			// 6
        //	| JBOX_IGNORELOCKCLICK	// 7
        //	| JBOX_HILITE			// 8
      //  | JBOX_BACKGROUND		// 9
        //	| JBOX_NOFLOATINSPECTOR	// 10
        // | c74::max::JBOX_TEXTFIELD		// 11
        //   | c74::max::JBOX_MOUSEDRAGDELTA	// 12
        //	| JBOX_COLOR			// 13
        //	| JBOX_BINBUF			// 14
        //	| JBOX_DRAWIOLOCKED		// 15
        //	| JBOX_DRAWBACKGROUND	// 16
        //	| JBOX_NOINSPECTFIRSTIN	// 17
        //	| JBOX_DEFAULTNAMES		// 18
        //	| JBOX_FIXWIDTH			// 19
        ;

    using tagged_attribute = std::pair<const symbol, attribute_base*>;

   
    template<int default_width_type = 20, int default_height_type = 20, long uiflags = defaultuiflags>
    class custom_ui_operator : public ui_operator_base {
    public:
        explicit custom_ui_operator(object_base* instance, const atoms& args)
            : m_instance{ instance }
        {
            if (!m_instance->maxobj()) // box will be a nullptr when being dummy-constructed
                return;


            long flags = uiflags;


            strings tags = instance->tags();
            auto tag_iter = std::find(tags.begin(), tags.end(), "multitouch");
            if (tag_iter != tags.end()) {
                flags |= JBOX_MULTITOUCH;
            }
            if (m_instance->has_mousedragdelta()) {
                flags |= JBOX_MOUSEDRAGDELTA;
            }
            if (m_instance->is_focusable()) {
                flags |= JBOX_HILITE;
            }

            const c74::max::t_atom* argv = args.empty() ? nullptr : &args[0];
            c74::max::jbox_new(reinterpret_cast<c74::max::t_jbox*>(m_instance->maxobj()), flags, static_cast<long>(args.size()), const_cast<max::t_atom*>(argv));
            reinterpret_cast<c74::max::t_jbox*>(m_instance->maxobj())->b_firstin = m_instance->maxobj();
        }

        virtual ~custom_ui_operator() {
            if (m_instance->maxobj())  // box will be a nullptr when being dummy-constructed
                jbox_free(reinterpret_cast<c74::max::t_jbox*>(m_instance->maxobj()));
        }

        void redraw() {
            if (m_instance->initialized())
                jbox_redraw(reinterpret_cast<c74::max::t_jbox*>(m_instance->maxobj()));
        }

        int default_width() const {
            return default_width_type;
        }

        int default_height() const {
            return default_height_type;
        }

        void add_color_attribute(const tagged_attribute a_color_attr) override {
            m_color_attributes.push_back(a_color_attr);
        }


        // update all style-aware attrs
        // must be done at the beginning of the Max object's "paint" method

        void update_colors() override {
            auto& self = *dynamic_cast<object_base*>(this);

            for (const auto& color_attr : m_color_attributes) {
                long              ac{ 4 };
                c74::max::t_atom avs[4];
                c74::max::t_atom* av = &avs[0];
                attribute_base& attr{ *color_attr.second };
                auto err = c74::max::object_attr_getvalueof(self, color_attr.first, &ac, &av);
                if (!err) {
                    atoms a{ av[0], av[1], av[2], av[3] };
                    attr.set(a, false); // notify must be false to prevent feedback loops
                }
            }
        }

    private:
        object_base* m_instance;
        vector<tagged_attribute> m_color_attributes;
    };


  
} // namespace c74::min
