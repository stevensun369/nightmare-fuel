#define GLM_FORCE_RADIANS
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "glew/glew.h"
#include "glfw/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Shaders/Utils/shaderLoader.hpp"

namespace {
constexpr float width = 1280, height = 720, pi = 3.14159265f;
using Vec = glm::vec2;
using Color = glm::vec4;
const Color background(.025f,.055f,.075f,1), floorColor(.07f,.14f,.16f,1);
const Color brass(.69f,.51f,.27f,1), cyan(.28f,.88f,.91f,1);
const Color cream(.95f,.88f,.69f,1), red(.90f,.22f,.23f,1);
float clamp(float n,float a,float b) { return std::max(a,std::min(b,n)); }
float dist(Vec a,Vec b) { return glm::length(a-b); }
Vec direction(Vec a,Vec b) { Vec d=b-a;return glm::length(d)>0.001f?glm::normalize(d):Vec(0); }
struct Rect {float x,y,w,h;};
const std::array<Rect,2> furniture {{{690,248,154,78},{846,510,144,68}}};
bool touches(Rect r,Vec p,float radius) {
    Vec q(clamp(p.x,r.x,r.x+r.w),clamp(p.y,r.y,r.y+r.h));
    return dist(p,q)<radius;
}
bool wallBetween(Vec a,Vec b,Rect r) {
    // Segment/rectangle intersection: furniture hides the player from the patrol.
    float lo=0,hi=1;
    for(int axis=0;axis<2;axis++) {
        float start=axis?a.y:a.x, delta=axis?b.y-a.y:b.x-a.x;
        float min=axis?r.y:r.x,max=min+(axis?r.h:r.w);
        if(std::abs(delta)<.0001f) {if(start<min||start>max)return false;}
        else {float u=(min-start)/delta,v=(max-start)/delta;
            if(u>v)std::swap(u,v);lo=std::max(lo,u);hi=std::min(hi,v);if(lo>hi)return false;}
    }
    return true;
}
bool visible(Vec a,Vec b) {for(auto r:furniture)if(wallBetween(a,b,r))return false;return true;}

struct Painter {
    GLuint program=0,quadVAO=0,quadVBO=0,circleVAO=0,circleVBO=0;
    GLint transform=-1,color=-1;
    glm::mat4 projection=glm::ortho(0.0f,width,height,0.0f,-1.0f,1.0f);
    void init() {
        program=LoadShaders("SimpleVertexShader.vert","SimpleFragmentShader.frag");
        transform=glGetUniformLocation(program,"transform");color=glGetUniformLocation(program,"color");
        const float quad[]={0,0,0, 1,0,0, 1,1,0, 0,0,0, 1,1,0, 0,1,0};
        glGenVertexArrays(1,&quadVAO);glGenBuffers(1,&quadVBO);
        glBindVertexArray(quadVAO);glBindBuffer(GL_ARRAY_BUFFER,quadVBO);
        glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);glEnableVertexAttribArray(0);
        std::vector<float> circle;
        for(int i=0;i<48;i++) {
            float a=2*pi*i/48,b=2*pi*(i+1)/48;
            circle.insert(circle.end(),{0,0,0,std::cos(a),std::sin(a),0,std::cos(b),std::sin(b),0});
        }
        glGenVertexArrays(1,&circleVAO);glGenBuffers(1,&circleVBO);
        glBindVertexArray(circleVAO);glBindBuffer(GL_ARRAY_BUFFER,circleVBO);
        glBufferData(GL_ARRAY_BUFFER,circle.size()*sizeof(float),circle.data(),GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
    void shape(GLuint vao,int count,Vec position,Vec scale,float angle,Color c) {
        glm::mat4 model(1);
        model=glm::translate(model,glm::vec3(position,0));
        model=glm::rotate(model,angle,glm::vec3(0,0,1));
        model=glm::scale(model,glm::vec3(scale,1));
        glm::mat4 mvp=projection*model;
        glUseProgram(program);glUniformMatrix4fv(transform,1,GL_FALSE,glm::value_ptr(mvp));
        glUniform4fv(color,1,glm::value_ptr(c));glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES,0,count);
    }
    void rect(float x,float y,float w,float h,Color c) {shape(quadVAO,6,Vec(x,y),Vec(w,h),0,c);}
    void box(Vec center,Vec size,float angle,Color c) {
        // Local rotation around the box center.
        Vec shift(-size.x/2,-size.y/2);
        Vec rotated(shift.x*std::cos(angle)-shift.y*std::sin(angle),shift.x*std::sin(angle)+shift.y*std::cos(angle));
        shape(quadVAO,6,center+rotated,size,angle,c);
    }
    void circle(Vec p,float radius,Color c) {shape(circleVAO,144,p,Vec(radius,radius),0,c);}
    void line(Vec a,Vec b,float thickness,Color c) {
        Vec d=b-a;box((a+b)*.5f,Vec(glm::length(d),thickness),std::atan2(d.y,d.x),c);
    }
    void ring(Vec p,float radius,float thickness,Color c) {
        for(int i=0;i<48;i++) {float a=2*pi*i/48,b=2*pi*(i+1)/48;
            line(p+Vec(std::cos(a),std::sin(a))*radius,p+Vec(std::cos(b),std::sin(b))*radius,thickness,c);}
    }
    void destroy(){glDeleteBuffers(1,&quadVBO);glDeleteBuffers(1,&circleVBO);
        glDeleteVertexArrays(1,&quadVAO);glDeleteVertexArrays(1,&circleVAO);glDeleteProgram(program);}
};

enum class Mode {Patrol,Notice,Chase,Stunned};
struct Game {
    Vec player{300,403},gear{670,403},enemy{915,403};
    float battery=100,gearAngle=0,empWave=10,stun=0,notice=0,patrolTime=0,hitFlash=0;
    bool locked=false,empUsed=false,lastEmp=false,won=false;
    Mode mode=Mode::Patrol;
    bool free(Vec p) const {
        if(p.x<91||p.x>1158||p.y<169||p.y>617)return false;
        for(auto r:furniture)if(touches(r,p,17))return false;
        return locked||dist(p,gear)>38;
    }
    void update(GLFWwindow* window,float dt) {
        dt=std::min(dt,.05f);empWave+=dt;hitFlash=std::max(0.0f,hitFlash-dt);
        if(won) {if(glfwGetKey(window,GLFW_KEY_ENTER)==GLFW_PRESS)*this=Game{};return;}
        Vec motion(0);
        if(glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS||glfwGetKey(window,GLFW_KEY_LEFT)==GLFW_PRESS)motion.x--;
        if(glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS||glfwGetKey(window,GLFW_KEY_RIGHT)==GLFW_PRESS)motion.x++;
        if(glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS||glfwGetKey(window,GLFW_KEY_UP)==GLFW_PRESS)motion.y--;
        if(glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS||glfwGetKey(window,GLFW_KEY_DOWN)==GLFW_PRESS)motion.y++;
        if(glm::length(motion)>0) {
            motion=glm::normalize(motion)*220.0f*dt;
            if(free(player+Vec(motion.x,0)))player.x+=motion.x;
            if(free(player+Vec(0,motion.y)))player.y+=motion.y;
        }
        bool pulling=glfwGetKey(window,GLFW_KEY_SPACE)==GLFW_PRESS||glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
        if(pulling&&!locked&&battery>0&&dist(player,gear)<370&&visible(player,gear)) {
            // Pull speed rises as the player gets closer; socket proximity snaps the gear in place.
            float speed=clamp((370-dist(player,gear))*1.3f,95,280);
            gear+=direction(gear,player)*speed*dt;
            gearAngle+=5*dt;battery=std::max(0.0f,battery-16*dt);
            if(dist(gear,Vec(450,403))<32) {gear=Vec(450,403);locked=true;battery=std::min(100.0f,battery+12);}
        } else battery=std::min(100.0f,battery+5*dt);
        if(glfwGetKey(window,GLFW_KEY_R)==GLFW_PRESS&&dist(player,Vec(265,565))<65)
            battery=std::min(100.0f,battery+85*dt);
        bool emp=glfwGetKey(window,GLFW_KEY_Q)==GLFW_PRESS||glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
        if(emp&&!lastEmp&&battery>=45) {
            battery-=45;empWave=0;
            if(dist(player,enemy)<185) {mode=Mode::Stunned;stun=3.5f;empUsed=true;}
        }
        lastEmp=emp;
        if(mode==Mode::Stunned) {stun-=dt;if(stun<=0)mode=Mode::Patrol;}
        else {
            bool sees=dist(player,enemy)<220&&visible(player,enemy);
            if(mode==Mode::Patrol) {
                patrolTime+=dt;
                Vec target(935+135*std::sin(patrolTime*.85f),400);
                enemy+=direction(enemy,target)*std::min(90*dt,dist(enemy,target));
                if(sees){mode=Mode::Notice;notice=0;}
            } else if(mode==Mode::Notice) {
                notice+=dt;
                if(!sees&&notice>.3f)mode=Mode::Patrol;
                else if(notice>.75f)mode=Mode::Chase;
            } else {
                if(sees)notice=1.4f;else notice-=dt;
                if(notice<=0)mode=Mode::Patrol;
                Vec next=enemy+direction(enemy,player)*125.0f*dt;
                bool blocked=false;for(auto r:furniture)blocked|=touches(r,next,23);
                if(!blocked&&next.x>90&&next.x<1160&&next.y>170&&next.y<615)enemy=next;
            }
            if(dist(player,enemy)<37) {
                player=Vec(300,403);enemy=Vec(915,403);mode=Mode::Patrol;
                battery=std::max(battery,55.0f);hitFlash=1;
            }
        }
        if(locked&&empUsed&&player.x>1123&&player.y>335&&player.y<473)won=true;
    }
    std::string objective()const {
        if(won)return "CURTAIN CALL - PRESS ENTER TO REPLAY";
        if(!locked)return "1/3 PULL THE GEAR INTO THE CYAN SOCKET";
        if(!empUsed)return "2/3 STUN THE BALLERINA WITH EMP";
        return "3/3 REACH THE OPEN EXIT";
    }
};

void gear(Painter& p,Vec at,float angle,bool locked) {
    p.circle(at+Vec(5,7),38,background);
    for(int i=0;i<12;i++) {
        float a=angle+i*pi/6;
        p.box(at+Vec(std::cos(a),std::sin(a))*32.0f,Vec(20,14),a,locked?brass:cyan);
    }
    p.circle(at,31,locked?brass:cyan);p.circle(at,22,background);
    p.circle(at,13,locked?brass:cyan);p.circle(at,7,background);
}
void mickey(Painter& p,Vec at,float time,bool pull) {
    p.circle(at+Vec(5,8),31,background);
    p.circle(at+Vec(-21,-18),16,background);p.circle(at+Vec(21,-18),16,background);
    p.circle(at,25,background);p.circle(at+Vec(0,6),15,cream);
    p.circle(at+Vec(-7,-5),3,cream);p.circle(at+Vec(7,-5),3,cream);
    p.circle(at+Vec(0,10),4,background);
    p.box(at+Vec(0,29),Vec(31,20),0,red);
    p.circle(at+Vec(24,21),9,cream);
    p.circle(at+Vec(27,18),6,pull?cyan:brass);
    if(pull)p.ring(at+Vec(27,18),12+2*std::sin(time*15),2,cyan);
}
void minnie(Painter& p,Vec at,float time,Mode mode) {
    bool stunned=mode==Mode::Stunned;
    float jerk=stunned?.18f*std::sin(time*29):.08f*std::sin(time*9);
    p.circle(at+Vec(5,12),35,background);
    for(int side:{-1,1}) {
        float swing=.4f*std::sin(time*(stunned?27:7)+side);
        p.box(at+Vec(side*23,21),Vec(8,30),side*swing,brass);
        p.box(at+Vec(side*25,-1),Vec(9,29),-side*swing,brass);
        p.circle(at+Vec(side*27,40),7,cream);
        p.circle(at+Vec(side*27,18),6,cream);
    }
    p.circle(at+Vec(0,5),22,red);
    p.circle(at+Vec(-21,-19),15,background);p.circle(at+Vec(21,-19),15,background);
    p.circle(at+Vec(0,-12),22,cream);p.circle(at+Vec(0,-12),16,background);
    p.circle(at+Vec(-7,-16+jerk*25),4,stunned?cyan:red);
    p.circle(at+Vec(7,-16-jerk*25),4,stunned?cyan:red);
    p.box(at+Vec(0,-37),Vec(23,10),jerk,red);
    if(stunned)p.ring(at,42,2,cyan);
}
void draw(Painter& p,const Game& g,float time,bool pulling) {
    p.rect(0,0,width,height,background);
    p.rect(57,139,1166,512,brass);p.rect(67,149,1146,492,floorColor);
    for(int y=0;y<10;y++)for(int x=0;x<23;x++)if((x+y)%2)
        p.rect(68+x*50,150+y*49,49,48,Color(.08f,.15f,.17f,1));
    p.rect(68,149,1145,25,background);p.rect(68,617,1145,24,background);
    for(int x=80;x<1200;x+=80) {p.rect(x,150,4,24,brass);p.rect(x,617,4,24,brass);}
    for(auto r:furniture) {
        p.rect(r.x+7,r.y+8,r.w,r.h,background);
        p.rect(r.x,r.y,r.w,r.h,brass);
        p.rect(r.x+7,r.y+7,r.w-14,r.h-14,Color(.13f,.32f,.33f,1));
    }
    p.line(Vec(450,403),Vec(1150,403),7,brass);
    p.circle(Vec(450,403),50,background);p.ring(Vec(450,403),48,7,brass);
    p.ring(Vec(450,403),37,2,cyan);
    p.rect(1145,337,61,132,brass);
    p.rect(1151,344,49,118,g.locked?cyan:red);
    if(g.locked)p.rect(1162,351,24,104,background);
    p.rect(238,533,55,67,brass);p.rect(244,539,43,55,background);
    p.line(Vec(254,564),Vec(276,564),4,cyan);p.line(Vec(276,564),Vec(263,580),4,cyan);
    p.ring(Vec(265,565),43,2,cyan);
    gear(p,g.locked?Vec(450,403):g.gear,g.gearAngle+(g.locked?0:.06f*std::sin(time*12)),g.locked);
    if(pulling&&!g.locked&&g.battery>0&&dist(g.player,g.gear)<370&&visible(g.player,g.gear))
        p.line(g.player,g.gear,2,cyan);
    if(g.empWave<.45f)p.ring(g.player,185*g.empWave/.45f,4,cyan);
    if(g.mode==Mode::Notice||g.mode==Mode::Chase)p.ring(g.enemy,65+4*std::sin(time*10),2,red);
    minnie(p,g.enemy,time,g.mode);mickey(p,g.player,time,pulling);
    p.rect(0,0,width,122,background);p.rect(0,119,width,3,brass);
    // Three objective lamps: gear, EMP, exit.
    for(int i=0;i<3;i++) {
        bool done=i==0?g.locked:i==1?g.empUsed:g.won;
        p.circle(Vec(90.0f+i*80,60),21,done?cyan:brass);
        p.circle(Vec(90.0f+i*80,60),13,background);
        if(i<2)p.line(Vec(111.0f+i*80,60),Vec(149.0f+i*80,60),3,brass);
    }
    p.rect(915,47,292,28,brass);
    p.rect(919,51,284,20,background);
    p.rect(922,54,278*g.battery/100,14,g.battery<45?red:cyan);
    p.rect(0,658,width,62,background);p.rect(0,658,width,3,brass);
    // Bottom legend uses symbols; the full controls remain in the window title and README.
    p.circle(Vec(78,688),9,cyan);p.line(Vec(110,688),Vec(170,688),3,cyan);
    p.ring(Vec(384,688),15,3,cyan);p.ring(Vec(659,688),15,3,red);
    p.rect(939,676,28,24,brass);
    if(g.hitFlash>0) {p.ring(g.player,44+g.hitFlash*12,5,red);}
    if(g.won) {
        p.rect(265,235,750,252,background);
        p.rect(272,242,736,238,brass);
        p.rect(280,250,720,222,background);
        p.ring(Vec(640,350),68+5*std::sin(time*3),5,cyan);
        p.circle(Vec(640,350),34,cyan);
        p.circle(Vec(628,339),9,background);p.circle(Vec(652,339),9,background);
    }
}
} // namespace

int main() {
    if(!glfwInit()) {std::fprintf(stderr,"GLFW initialization failed\n");return 1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
    GLFWwindow* window=glfwCreateWindow(1280,720,"Midnight Encore",nullptr,nullptr);
    if(!window) {glfwTerminate();return 1;}
    glfwMakeContextCurrent(window);glfwSwapInterval(1);glewExperimental=GL_TRUE;
    if(glewInit()!=GLEW_OK){std::fprintf(stderr,"GLEW initialization failed\n");glfwDestroyWindow(window);glfwTerminate();return 1;}
    glGetError();
    Painter painter;painter.init();Game game;
    std::fprintf(stderr,"GL: %s | %s | program=%u uniforms=%d,%d\n",
        glGetString(GL_VERSION),glGetString(GL_RENDERER),painter.program,painter.transform,painter.color);
    double previous=glfwGetTime();std::string oldTitle;
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();double now=glfwGetTime();float dt=static_cast<float>(now-previous);previous=now;
        if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS)glfwSetWindowShouldClose(window,true);
        game.update(window,dt);
        std::string title="MIDNIGHT ENCORE  |  "+game.objective()+"  |  WASD MOVE  SPACE PULL  Q EMP  R CHARGE";
        if(title!=oldTitle){glfwSetWindowTitle(window,title.c_str());oldTitle=title;}
        int fw,fh;glfwGetFramebufferSize(window,&fw,&fh);if(fw<=0||fh<=0)continue;
        float s=std::min(fw/width,fh/height);int vw=int(width*s),vh=int(height*s);
        glViewport(0,0,fw,fh);glClearColor(background.r,background.g,background.b,1);glClear(GL_COLOR_BUFFER_BIT);
        glViewport((fw-vw)/2,(fh-vh)/2,vw,vh);
        bool pull=glfwGetKey(window,GLFW_KEY_SPACE)==GLFW_PRESS||glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
        draw(painter,game,static_cast<float>(now),pull);
        static bool checked=false;
        if(!checked) {
            unsigned char pixel[4]{};
            glReadPixels(fw/2,fh/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
            std::fprintf(stderr,"GL first frame: pixel=%u,%u,%u,%u error=0x%x\n",
                pixel[0],pixel[1],pixel[2],pixel[3],glGetError());
            checked=true;
        }
        glfwSwapBuffers(window);
    }
    painter.destroy();glfwDestroyWindow(window);glfwTerminate();return 0;
}
