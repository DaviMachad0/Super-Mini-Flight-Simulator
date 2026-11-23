// Como compilar:
// g++ Projeto_Flight_Sim.cpp -o fase3 -lglfw -lGL -lGLU

// Como executar:
// ./fase3

// Controles:
// T: decolar
// L: aterrissar
// R: alterna sólido/malha
// ESC: sair
// Setas cima/baixo: acelera/desacelera (em voo)
// Setas esquerda/direita: gira o avião (em voo)

#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PI 3.14159265358979323846

// Parâmetros do planeta e do sol
float raioPlaneta;
float posSol[3];

// Estado do avião
// upAviao é a "fonte da verdade": aponta do centro do planeta para o avião.
// posAviao é derivada dele: posAviao = upAviao * (raioPlaneta + altitude + alturaOlho)
float posAviao[3]     = {0.0f, 0.0f, 0.0f};
float direcaoAviao[3] = {1.0f, 0.0f, 0.0f};
float upAviao[3]      = {0.0f, 1.0f, 0.0f};

float velocidade        = 0.0f;
const float velocidadeInicial = 0.6f;  // velocidade ao terminar a decolagem
const float velocidadeMin     = 0.2f;  // mínimo em voo, para não "parar no ar"
const float velocidadeMax     = 3.0f;
const float aceleracao        = 0.8f;  // quanto a velocidade muda por segundo segurando a seta
const int maxFatias = 360;
const int maxPilhas = 180;
const int maxVertices = (maxPilhas + 1) * (maxFatias + 1);
const int maxIndices = maxPilhas * maxFatias * 6;

unsigned char* mapaAltura = NULL;
int mapaLarg = 0;
int mapaAlt = 0;
float relevoMax = 0.0f;
float relevoSuave = 0.0f;
float altitude         = 0.0f;
float altitudeAlvo     = 2.0f;
float velocidadeSubida = 1.0f;
const float alturaOlho = 0.05f; // evita a câmera ficar exatamente na superfície no chão

float ultimoFrame = 0.0f;

bool modoMalha   = false; // Estado do modo de renderização (Sólido ou Malha)
bool decolando   = false;
bool aviaoVoando = false;
bool pousando    = false;
const float velocidadeGiro = 1.2f; // rad/s

// ---------- Funções auxiliares de vetores ----------

// Produto vetorial: r = a x b
void produtoVetorial(const float a[3], const float b[3], float r[3]) {
    r[0] = a[1]*b[2] - a[2]*b[1];
    r[1] = a[2]*b[0] - a[0]*b[2];
    r[2] = a[0]*b[1] - a[1]*b[0];
}

// Produto escalar: a . b
float produtoEscalar(const float a[3], const float b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

// Deixa o vetor com comprimento 1
void normalizar(float v[3]) {
    float n = sqrtf(produtoEscalar(v, v));
    if (n > 1e-6f) {
        v[0] /= n; v[1] /= n; v[2] /= n;
    }
}

// Função para rotacionar um vetor em torno de um eixo unitário (fórmula de Rodrigues)
void rotacionar(float v[3], const float eixo[3], float ang) {
    float c = cosf(ang), s = sinf(ang);
    float d = produtoEscalar(eixo, v);
    float cr[3];
    produtoVetorial(eixo, v, cr);
    for (int i = 0; i < 3; i++)
        v[i] = v[i]*c + cr[i]*s + eixo[i]*d*(1.0f - c);
}

// Corrige o acúmulo de erro numérico: up e direção unitários e perpendiculares (Gram-Schmidt)
void ortonormalizar() {
    normalizar(upAviao);
    float d = produtoEscalar(direcaoAviao, upAviao);
    for (int i = 0; i < 3; i++)
        direcaoAviao[i] -= d * upAviao[i];
    normalizar(direcaoAviao);
}

bool carregarMapaAltura(const char* arquivo) {
    int canais;
    mapaAltura = stbi_load(arquivo, &mapaLarg, &mapaAlt, &canais, 1);
    if (!mapaAltura){
        std::cerr << "Erro ao carregar " << arquivo << "\n";
        return false;
    }
    return true;
}

// Altura normalizada do mapa
float alturaMapa(float u, float v) {
    if (!mapaAltura) return 0.0f;
    u = u - floorf(u);
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    float x = u * mapaLarg;
    float y = v * (mapaAlt - 1);
    int x0 = (int)x;
    int y0 = (int)y;
    float fx = x - x0;
    float fy = y - y0;
    int x1 = (x0 + 1) %mapaLarg;
    x0 = x0 % mapaLarg;
    int y1 = (y0 + 1 < mapaAlt) ? y0 + 1 : y0;

    float a = mapaAltura[y0 * mapaLarg + x0];
    float b = mapaAltura[y0 * mapaLarg + x1];
    float c = mapaAltura[y1 * mapaLarg + x0];
    float d = mapaAltura[y1 * mapaLarg + x1];

    float linha0 = a * (1.0f - fx) + b * fx;
    float linha1 = c * (1.0f - fx) + d * fx;
    return (linha0 * (1.0f - fy) + linha1 * fy) / 255.0f;
}

// Relevo na direção dir
// Inverso da formula usada em gerarEsfera
float relevoNaDirecao(const float dir[3]) {
    float lat = asinf(dir[1]);
    float lng = atan2f(-dir[2], dir[0]);
    float u = lng / (2.0f * PI);
    if (u < 0.0f) u += 1.0f;
    float v = lat / PI + 0.5f;
    return relevoMax * alturaMapa(u, v);
}

float distanciaDoCentro() {
    return raioPlaneta + relevoSuave + altitude + alturaOlho;
}

// Move o avião para frente sobre a esfera.
// Andar para frente = rotacionar up e direção em torno do eixo (up x direção),
// pelo ângulo theta = arco / raio = (velocidade * dt) / (R + h)
void moverParaFrente(float dt) {
    if (velocidade <= 0.0f) return;

    float eixo[3];
    produtoVetorial(upAviao, direcaoAviao, eixo);
    normalizar(eixo);

    float distanciaCentro = distanciaDoCentro();
    float theta = velocidade * dt / distanciaCentro;

    rotacionar(upAviao, eixo, theta);
    rotacionar(direcaoAviao, eixo, theta);
}

struct Malha {
    float vertices[maxVertices * 3];
    float normais[maxVertices * 3];
    float texcoords[maxVertices * 2];
    unsigned int indices[maxIndices];
    int numIndices;
};

Malha malhaPlaneta;
Malha malhaSol;

// Função para gerar uma esfera com polos no eixo y
void gerarEsfera(Malha& m, float raio, int fatias, int pilhas, bool usaRelevo) {
    if (fatias > maxFatias) fatias = maxFatias;
    if (pilhas > maxPilhas) pilhas = maxPilhas;

    int v = 0, t = 0, k = 0;

    // Vertices: (pilhas + 1) aneis de latitude, cada um com (fatias + 1) pontos
    for (int i = 0; i <= pilhas; i++) {
        float lat = PI * (-0.5f + (float)i / pilhas);
        float vTex = (float)i / pilhas;
        for (int j = 0; j <= fatias; j++) {
            float lng = 2.0f * PI * (float)j / fatias;
            float uTex = (float)j / fatias;

            float dx = cosf(lat) * cosf(lng);
            float dy = sinf(lat);
            float dz = -cosf(lat) * sinf(lng);

            float r = raio;
            if (usaRelevo) {
                float uMapa = (i == 0 || i == pilhas) ? 0.0f : uTex;
                r += relevoMax * alturaMapa(uMapa, vTex);
            }

            m.vertices[v] = dx * r;
            m.vertices[v + 1] = dy * r;
            m.vertices[v + 2] = dz * r;

            m.normais[v] = dx;
            m.normais[v + 1] = dy;
            m.normais[v + 2] = dz;
            v += 3;

            m.texcoords[t++] = uTex;
            m.texcoords[t++] = vTex;
        }
    }

    // Indices: cada "quadrado" entre dois aneis vira 2 triangulos
    for (int i = 0; i < pilhas; i++) {
        for (int j = 0; j < fatias; j++) {
            unsigned int a = i * (fatias + 1) + j;
            unsigned int b = a + fatias + 1;

            m.indices[k++] = a;
            m.indices[k++] = b;
            m.indices[k++] = a + 1;

            m.indices[k++] = a + 1;
            m.indices[k++] = b;
            m.indices[k++] = b + 1;
        }
    }
    m.numIndices = k;

    if (usaRelevo) {
        int numVert = (pilhas + 1) * (fatias + 1);
        for (int q = 0; q < numVert * 3; q++) m.normais[q] = 0.0f;

        for (int q = 0; q < k; q += 3) {
            int idx[3] = { (int)m.indices[q] * 3, (int)m.indices[q + 1] * 3, (int)m.indices[q + 2] * 3 };
            float e1[3], e2[3], nf[3];
            for (int c = 0; c < 3; c++) {
                e1[c] = m.vertices[idx[1] + c] - m.vertices[idx[0] + c];
                e2[c] = m.vertices[idx[2] + c] - m.vertices[idx[0] + c];
            }
            produtoVetorial(e1, e2, nf);
            for (int w = 0; w < 3; w++)
                for (int c = 0; c < 3; c++)
                    m.normais[idx[w] + c] += nf[c];
        }

        for (int i = 0; i <= pilhas; i++) {
            int a = i * (fatias + 1) * 3;
            int b = a + fatias * 3;
            for (int c = 0; c < 3; c++) {
                float soma = m.normais[a + c] + m.normais[b + c];
                m.normais[a + c] = soma;
                m.normais[b + c] = soma;
            }
        }

        for (int q = 0; q < numVert; q++) {
            float* n = &m.normais[q * 3];
            const float* p = &m.vertices[q * 3];
            normalizar(n);
            if (produtoEscalar(n, p) < 0.0f) {
                n[0] = -n[0];
                n[1] = -n[1];
                n[2] = -n[2];
            }
        }

        for (int j = 0; j <= fatias; j++) {
            int sul = j * 3;
            int norte = (pilhas * (fatias + 1) + j) * 3;
            m.normais[sul] = 0.0f;
            m.normais[sul + 1] = -1.0f;
            m.normais[sul + 2] = 0.0f;
            m.normais[norte] = 0.0f;
            m.normais[norte + 1] = 1.0f;
            m.normais[norte + 2] = 0.0f;
        }
    }
}

// Função para desenhar a malha com vertex arrays
void desenharMalha(const Malha& m) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, m.vertices);
    glNormalPointer(GL_FLOAT, 0, m.normais);
    glTexCoordPointer(2, GL_FLOAT, 0, m.texcoords);

    glDrawElements(GL_TRIANGLES, m.numIndices, GL_UNSIGNED_INT, m.indices);

    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

// Função para lidar com o teclado, utilizando a tecla R para alternar entre os modos
void teclado(GLFWwindow* janela, int tecla, int scancode, int acao, int mods) {
    if (acao == GLFW_PRESS) {
        if (tecla == GLFW_KEY_R) {
            modoMalha = !modoMalha;
            if (modoMalha) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Modo malha
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Modo sólido
            }
        }
        if (tecla == GLFW_KEY_T && !aviaoVoando && !decolando) {
            decolando = true;
            pousando = false;
        }
        if (tecla == GLFW_KEY_L && (aviaoVoando || decolando)) {
            pousando = true;
            decolando = false;
            aviaoVoando = false;
        }
        if (tecla == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(janela, GLFW_TRUE);
        }
    }
}

// Redimensionador da janela
void redimensionar(GLFWwindow* janela, int largura, int altura) {
    if (altura == 0) altura = 1;
    float aspecto = (float)largura / (float)altura;

    glViewport(0, 0, largura, altura);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // FOV vertical de 60 graus: largo o bastante para ver o planeta (abaixo) e o sol (perto do horizonte) ao mesmo tempo
    gluPerspective(60.0, aspecto, 0.01, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

// Inicializador do OpenGL
void iniciar(GLFWwindow* janela) {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // Fundo escuro imitando o espaço
    glEnable(GL_DEPTH_TEST);

    int largura, altura;
    glfwGetFramebufferSize(janela, &largura, &altura);
    redimensionar(janela, largura, altura);

    // Liga o shading
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); // glColor passa a definir o material
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    float ambiente[] = {0.25f, 0.25f, 0.25f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambiente);

    float difusa[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, difusa);
}

// Função de desenho da cena
void desenhar() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Inclinação da câmera para baixo: no chão olha quase reto (vê o horizonte),
    // e vai inclinando conforme sobe, para o planeta ocupar boa parte da tela em voo.
    float fracaoAltura = (altitudeAlvo > 0.0f) ? altitude / altitudeAlvo : 0.0f;
    float olharBaixo = 0.3f + 0.15f * fracaoAltura; // de ~17 graus (chão) a ~24 graus (cruzeiro) para baixo

    // Câmera na posição do avião, olhando para frente e um pouco para baixo
    gluLookAt(
        posAviao[0], posAviao[1], posAviao[2],

        posAviao[0] + direcaoAviao[0] - olharBaixo * upAviao[0],
        posAviao[1] + direcaoAviao[1] - olharBaixo * upAviao[1],
        posAviao[2] + direcaoAviao[2] - olharBaixo * upAviao[2],

        upAviao[0], upAviao[1], upAviao[2]
    );

    // Posicionamento da luz exatamente na posição do sol
    float posLuz[] = {posSol[0], posSol[1], posSol[2], 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, posLuz);

    // Renderização do Planeta (Esfera na origem 0,0,0)
    glPushMatrix();
    glColor3f(0.2f, 0.5f, 0.9f);
    desenharMalha(malhaPlaneta);
    glPopMatrix();

    // Renderização do Sol (Esfera na posição definida pelo usuário)
    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(posSol[0], posSol[1], posSol[2]);
    glColor3f(1.0f, 1.0f, 0.0f);
    desenharMalha(malhaSol);
    glPopMatrix();
    glEnable(GL_LIGHTING);
}

int main() {

    // Leitura dos parâmetros do planeta e do sol
    std::cout << "Raio do planeta (recomendado: 2): ";
    std::cin >> raioPlaneta;
    std::cout << "Posicao do Sol (recomendado: 8 4 5): ";
    std::cin >> posSol[0] >> posSol[1] >> posSol[2];

    // raioPlaneta deve ser positivo
    if (!std::cin || raioPlaneta <= 0) {
        std::cerr << "Entrada invalida.\n";
        return -1;
    }

    // Altitude de voo proporcional ao raio: o planeta é pequeno, então voando
    // baixo ele ocupa boa parte da tela (com R = 2, h = 0.3).
    // A subida leva 1.5 s, para dar tempo de ver o chão se afastando.
    altitudeAlvo     = 0.15f * raioPlaneta;
    velocidadeSubida = altitudeAlvo / 1.5f;

    // Inicialização do GLFW
    if (!glfwInit()) {
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1200, 800, "Super Mini Flight Simulator", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Registro de callbacks de eventos (teclado e redimensionamento)
    glfwSetKeyCallback(window, teclado);
    glfwSetFramebufferSizeCallback(window, redimensionar);

    iniciar(window);
    relevoMax = 0.08f * raioPlaneta;

    stbi_set_flip_vertically_on_load(1);
    if (!carregarMapaAltura("HeightMap/altura.jpg")) return -1;

    gerarEsfera(malhaPlaneta, raioPlaneta, 360, 180, true);
    gerarEsfera(malhaSol, 1.0f, 40, 40, false);

    relevoSuave = relevoNaDirecao(upAviao);
    ultimoFrame = (float)glfwGetTime();

    // Loop principal de renderização
    while (!glfwWindowShouldClose(window)) {

        float tempoAtual = (float)glfwGetTime();
        float dt = tempoAtual - ultimoFrame;
        ultimoFrame = tempoAtual;

        // Decolagem: sobe parado até a altitude alvo, depois parte com a velocidade inicial
        if (decolando) {
            altitude += velocidadeSubida * dt;
            if (altitude >= altitudeAlvo) {
                altitude = altitudeAlvo;
                decolando = false;
                aviaoVoando = true;
                velocidade = velocidadeInicial;
            }
        }

        // Aterrissagem: a altura diminui até o chão; ao tocar o solo, o avião para
        if (pousando) {
            altitude -= velocidadeSubida * dt;
            if (altitude <= 0.0f) {
                altitude = 0.0f;
                pousando = false;
                velocidade = 0.0f;
            }
        }

        // Controles de voo (só em cruzeiro)
        if (aviaoVoando) {
            // Setas cima/baixo: velocidade
            if (glfwGetKey(window, GLFW_KEY_UP)   == GLFW_PRESS) velocidade += aceleracao * dt;
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) velocidade -= aceleracao * dt;
            if (velocidade < velocidadeMin) velocidade = velocidadeMin;
            if (velocidade > velocidadeMax) velocidade = velocidadeMax;

            // Setas esquerda/direita: rotação em torno do vetor up
            float giro = 0.0f;
            if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) giro += velocidadeGiro * dt;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) giro -= velocidadeGiro * dt;
            if (giro != 0.0f) {
                rotacionar(direcaoAviao, upAviao, giro);
            }
        }

        // Movimento para frente sobre a esfera (em cruzeiro e durante o pouso)
        moverParaFrente(dt);
        ortonormalizar();

        float relevoAlvo = relevoNaDirecao(upAviao);
        relevoSuave += (relevoAlvo - relevoSuave) * fminf(1.0f, 6.0f * dt);

        // A posição sai do vetor up: sempre a (R + h) do centro do planeta
        float distanciaCentro = distanciaDoCentro();
        for (int i = 0; i < 3; i++)
            posAviao[i] = upAviao[i] * distanciaCentro;

        desenhar();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}