// Small deterministic CLAP effect used only by the integration tests.
#include <clap/clap.h>
#include <cstring>
#include <new>
#include <atomic>
#include <cmath>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace
{
const char* features[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
const clap_plugin_descriptor_t descriptor {CLAP_VERSION,"org.dtx.test.gain","DTX Test Gain","DTX Tests","","","","1.0.0","Test fixture",features};
struct Gain
{
    clap_plugin_t plugin {};
    std::atomic<double> gain {.5};
    HWND window=nullptr;
    uint32_t width=320,height=200;
    Gain()
    {
        plugin.desc=&descriptor;plugin.plugin_data=this;
        plugin.init=[](const clap_plugin_t*){return true;};
        plugin.destroy=[](const clap_plugin_t* p){delete static_cast<Gain*>(p->plugin_data);};
        plugin.activate=[](const clap_plugin_t*,double,uint32_t,uint32_t){return true;};
        plugin.deactivate=[](const clap_plugin_t*){};plugin.start_processing=[](const clap_plugin_t*){return true;};plugin.stop_processing=[](const clap_plugin_t*){};plugin.reset=[](const clap_plugin_t*){};
        plugin.process=[](const clap_plugin_t* p,const clap_process_t* block)->clap_process_status {
            auto& self=*static_cast<Gain*>(p->plugin_data);
            uint32_t event=0,total=block->in_events->size(block->in_events);
            for(uint32_t frame=0;frame<block->frames_count;++frame)
            {
                while(event<total)
                {
                    auto* h=block->in_events->get(block->in_events,event);if(h->time>frame)break;
                    if(h->type==CLAP_EVENT_PARAM_VALUE&&h->size>=sizeof(clap_event_param_value_t))self.gain.store(reinterpret_cast<const clap_event_param_value_t*>(h)->value);++event;
                }
                for(uint32_t c=0;c<2;++c)block->audio_outputs[0].data32[c][frame]=block->audio_inputs[0].data32[c][frame]*(float)self.gain.load();
            }
            return CLAP_PROCESS_CONTINUE;
        };
        plugin.on_main_thread=[](const clap_plugin_t*){};
        plugin.get_extension=[](const clap_plugin_t* p,const char* id)->const void*{
            static const clap_plugin_gui_t gui {
                [](const clap_plugin_t*,const char* api,bool floating){return !floating&&api&&std::strcmp(api,CLAP_WINDOW_API_WIN32)==0;},
                [](const clap_plugin_t*,const char** api,bool* floating){*api=CLAP_WINDOW_API_WIN32;*floating=false;return true;},
                [](const clap_plugin_t* p,const char*,bool){auto* s=static_cast<Gain*>(p->plugin_data);s->window=CreateWindowExW(0,L"STATIC",L"DTX CLAP GUI TEST\n320 x 200 native editor",WS_POPUP|SS_CENTER,0,0,320,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);return s->window!=nullptr;},
                [](const clap_plugin_t* p){auto* s=static_cast<Gain*>(p->plugin_data);DestroyWindow(s->window);s->window=nullptr;},
                [](const clap_plugin_t*,double){return true;},
                [](const clap_plugin_t* p,uint32_t* w,uint32_t* h){auto* s=static_cast<Gain*>(p->plugin_data);*w=s->width;*h=s->height;return true;},
                [](const clap_plugin_t*){return true;},
                [](const clap_plugin_t*,clap_gui_resize_hints_t*){return false;},
                [](const clap_plugin_t*,uint32_t*,uint32_t*){return true;},
                [](const clap_plugin_t* p,uint32_t w,uint32_t h){auto* s=static_cast<Gain*>(p->plugin_data);s->width=w;s->height=h;SetWindowPos(s->window,nullptr,0,0,(int)w,(int)h,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);return true;},
                [](const clap_plugin_t* p,const clap_window_t* parent){auto* s=static_cast<Gain*>(p->plugin_data);SetWindowLongPtrW(s->window,GWL_STYLE,WS_CHILD|SS_CENTER);SetParent(s->window,(HWND)parent->win32);SetWindowPos(s->window,nullptr,0,0,(int)s->width,(int)s->height,SWP_NOZORDER|SWP_NOACTIVATE);return true;},
                [](const clap_plugin_t*,const clap_window_t*){return false;},
                [](const clap_plugin_t*,const char*){},
                [](const clap_plugin_t* p){ShowWindow(static_cast<Gain*>(p->plugin_data)->window,SW_SHOWNA);return true;},
                [](const clap_plugin_t* p){ShowWindow(static_cast<Gain*>(p->plugin_data)->window,SW_HIDE);return true;}};
            static const clap_plugin_audio_ports_t ports {
                [](const clap_plugin_t*,bool)->uint32_t{return 1;},
                [](const clap_plugin_t*,uint32_t index,bool input,clap_audio_port_info_t* info){if(index)return false;*info={};info->id=input?0:1;std::strcpy(info->name,input?"Input":"Output");info->channel_count=2;info->flags=CLAP_AUDIO_PORT_IS_MAIN;info->port_type=CLAP_PORT_STEREO;info->in_place_pair=CLAP_INVALID_ID;return true;}};
            static const clap_plugin_params_t params {
                [](const clap_plugin_t*)->uint32_t{return 1;},
                [](const clap_plugin_t*,uint32_t index,clap_param_info_t* info){if(index)return false;*info={};info->id=77;info->flags=CLAP_PARAM_IS_AUTOMATABLE;std::strcpy(info->name,"Gain");info->min_value=0;info->max_value=2;info->default_value=.5;return true;},
                [](const clap_plugin_t* p,clap_id id,double* v){if(id!=77)return false;*v=static_cast<Gain*>(p->plugin_data)->gain.load();return true;},
                [](const clap_plugin_t*,clap_id,double,char*,uint32_t){return false;},
                [](const clap_plugin_t*,clap_id,const char*,double*){return false;},
                [](const clap_plugin_t* p,const clap_input_events_t* in,const clap_output_events_t*){for(uint32_t i=0;i<in->size(in);++i){auto* e=in->get(in,i);if(e->type==CLAP_EVENT_PARAM_VALUE)static_cast<Gain*>(p->plugin_data)->gain.store(reinterpret_cast<const clap_event_param_value_t*>(e)->value);}}};
            static const clap_plugin_state_t state {
                [](const clap_plugin_t* p,const clap_ostream_t* out){double v=static_cast<Gain*>(p->plugin_data)->gain.load();return out->write(out,&v,sizeof(v))==sizeof(v);},
                [](const clap_plugin_t* p,const clap_istream_t* in){double v=0;if(in->read(in,&v,sizeof(v))!=sizeof(v)||!std::isfinite(v))return false;static_cast<Gain*>(p->plugin_data)->gain.store(v);return true;}};
            if(std::strcmp(id,CLAP_EXT_AUDIO_PORTS)==0)return &ports;
            if(std::strcmp(id,CLAP_EXT_PARAMS)==0)return &params;
            if(std::strcmp(id,CLAP_EXT_GUI)==0)return &gui;
            if(std::strcmp(id,"org.dtx.test.gui-window")==0)return &static_cast<Gain*>(p->plugin_data)->window;
            if(std::strcmp(id,CLAP_EXT_STATE)==0)return &state;return nullptr;
        };
    }
};
const clap_plugin_factory_t factory {
    [](const clap_plugin_factory_t*)->uint32_t{return 1;},
    [](const clap_plugin_factory_t*,uint32_t i)->const clap_plugin_descriptor_t*{return i==0?&descriptor:nullptr;},
    [](const clap_plugin_factory_t*,const clap_host_t*,const char* id)->const clap_plugin_t*{if(std::strcmp(id,descriptor.id)!=0)return nullptr;auto* self=new(std::nothrow) Gain;return self?&self->plugin:nullptr;}};
}
extern "C" __declspec(dllexport) const clap_plugin_entry_t clap_entry {
    CLAP_VERSION,[](const char*){return true;},[](){},[](const char* id)->const void*{return std::strcmp(id,CLAP_PLUGIN_FACTORY_ID)==0?&factory:nullptr;}};
