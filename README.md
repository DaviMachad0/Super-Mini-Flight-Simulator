# Super Mini Flight Simulator

Simulador de voo em primeira pessoa sobre um planeta pequeno e redondo, feito em **C++ com OpenGL (GLFW + GLU)**. A câmera é a de um avião que decola, sobrevoa o planeta (com relevo, iluminação e texturas) e aterrissa.

Projeto da disciplina de Computação Gráfica (UFS).

## O que o projeto faz

**Fase 1: planeta e sol**
- Esfera do planeta na origem e esfera amarela do sol na posição dada pelo usuário.
- Raio do planeta e posição do sol são lidos no terminal.
- Tecla `R` alterna entre modo sólido e modo malha (wireframe).
- Esferas geradas utilizando **gluSphere**

**Fase 2: o avião**
- Esferas geradas manualmente com **vertex arrays**
- Câmera em primeira pessoa, olhando um pouco para baixo.
- Decolagem e aterrissagem animadas (o chão se afasta e se aproxima).
- Controle de velocidade e giro em torno do vetor *up* do avião.
- Movimento sobre a esfera por rotações, mantendo sempre a mesma distância do solo.

**Fase 3: upgrade do planeta**
- **Shading:** o sol é uma fonte de luz pontual, com luz ambiente para o lado escuro não ficar totalmente preto.
- **Relevo:** cada vértice do planeta é deslocado pelo valor de um **heightmap** em tons de cinza, e as normais são recalculadas a partir dos triângulos.
- **Piloto automático:** a altitude do avião soma o relevo sob ele, mantendo a distância `h` do solo.
- **Texturas de cor** no planeta e no sol.

## Requisitos

- Compilador C++ (`g++`)
- OpenGL, GLFW 3 e GLU

## Estrutura de arquivos

```
.
├── Projeto_Flight_Sim.cpp   # código-fonte
├── stb_image.h              # biblioteca de leitura de imagens
├── HeightMap/
│   └── altura.jpg
├── Texturas/
│   ├── terra.jpg
│   └── sol.jpg
└── README.md
```

As pastas contendo os arquivos de imagem precisam estar **na pasta de onde o programa é executado**, porque são abertos por caminho relativo.

## Como compilar e executar

```bash
Make run
```

Não há bibliotecas extras para linkar: `stb_image.h` é apenas um cabeçalho. Podem aparecer avisos (*warnings*) vindos dele na compilação, que são inofensivos.

Ao iniciar, o programa pede:

```
Raio do planeta:
Posicao do Sol:
```

Se algum arquivo de imagem estiver faltando, o programa informa `Erro ao carregar ...` no terminal.

## Controles

| Tecla | Ação |
|---|---|
| `T` | Decolar |
| `L` | Aterrissar |
| `↑` / `↓` | Aumenta / diminui a velocidade (em voo) |
| `←` / `→` | Gira o avião para a esquerda / direita (em voo) |
| `R` | Alterna entre modo sólido e malha |
| `ESC` | Sair |

## Como funciona

- **Estado do avião:** guardado em dois vetores, `upAviao` (do centro do planeta para o avião, ou seja, a normal da esfera) e `direcaoAviao` (tangente à esfera). A posição é sempre `upAviao * (raio + relevo + altitude)`.
- **Movimento:** andar para frente é girar `up` e `direcao` juntos em torno do eixo `up × direcao`, pelo ângulo `velocidade·dt / distância ao centro` (fórmula de Rodrigues). A cada frame os vetores são reortogonalizados (Gram-Schmidt) para evitar acúmulo de erro numérico.
- **Giro:** rotação da direção em torno do vetor `up`.
- **Câmera:** `gluLookAt` na posição do avião, olhando para `posição + direção − k·up`, com `k` crescendo levemente com a altitude.
- **Malha da esfera:** anéis de latitude × fatias de longitude, desenhada com `glDrawElements`. Os mesmos `(u, v)` de cada vértice servem para amostrar o heightmap.
- **Relevo:** `raio_do_vértice = raio + relevoMax · altura(u, v)`, com interpolação bilinear. As normais são a soma das normais dos triângulos vizinhos (com costura na longitude e tratamento dos polos).
- **Altura do avião:** o relevo sob o avião é amostrado a partir de `upAviao` e suavizado no tempo, para a câmera não tremer nas montanhas.
- **Luz:** `GL_LIGHT0` posicionada nas coordenadas do sol, depois do `gluLookAt`; a esfera do sol é desenhada sem iluminação.

## Parâmetros que podem ser ajustados

| Onde | O quê |
|---|---|
| `relevoMax` (na `main`) | Exagero do relevo (hoje 8% do raio) |
| `altitudeAlvo` (na `main`) | Altura de cruzeiro (hoje 15% do raio) |
| Luz ambiente (`iniciar`) | Quão visível é o lado escuro |
| `gluPerspective` (`redimensionar`) | Campo de visão (hoje 60°) |
| `olharBaixo` (`desenhar`) | Inclinação da câmera para baixo |
| `velocidadeInicial`, `velocidadeMin`, `velocidadeMax`, `aceleracao` | Comportamento da velocidade |
| Resolução em `gerarEsfera(...)` | Qualidade da malha (limite em `maxFatias`/`maxPilhas`) |

## Observações

- O avião começa no polo norte (`upAviao = (0, 1, 0)`), que é só oceano plano. Para começar sobre um continente, altere `upAviao` inicial e chame `ortonormalizar()` antes do loop.
- O mapa de altura usa a projeção equirretangular (proporção 2:1, norte em cima, Greenwich ao centro).

## Créditos

- Texturas de cor da Terra e do Sol: [Solar System Scope](https://www.solarsystemscope.com/textures/).
- Mapa de altura: SRTM, NASA (`Srtm_ramp2.world.21600x10800.jpg`, reduzido para 2048x1024).
- Leitura de imagens: [stb_image](https://github.com/nothings/stb), de Sean Barrett (domínio público).

## Autores

- Davi Oliveira Machado
- Jeferson Gonzaga dos Santos Filho
