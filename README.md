# Resource Monitoring System for Linux Containers

## Descrição do Projeto

Este projeto implementa um sistema abrangente de monitoramento e análise de recursos para Linux, explorando os mecanismos fundamentais que tornam a containerização possível: **namespaces** e **control groups (cgroups)**. O sistema é composto por três componentes principais que coletam métricas detalhadas, analisam isolamento e gerenciam limitações de recursos.

### Componentes Principais

1. **Resource Profiler**: Coleta e reporta métricas detalhadas de processos (CPU, memória, I/O, rede)
2. **Namespace Analyzer**: Analisa e reporta isolamento via namespaces do Linux
3. **Control Group Manager**: Gerencia e monitora limitações de recursos via cgroups

### Experimentos Implementados

O projeto inclui 5 experimentos obrigatórios que demonstram aspectos fundamentais do gerenciamento de recursos:

- **Experimento 1**: Overhead de Monitoramento
- **Experimento 2**: Isolamento via Namespaces  
- **Experimento 3**: Throttling de CPU
- **Experimento 4**: Limitação de Memória
- **Experimento 5**: Limitação de I/O

## Requisitos e Dependências

### Requisitos do Sistema

- **Sistema Operacional**: Linux (testado em Ubuntu 24.04+)
- **Kernel**: Linux 4.15+ com suporte a cgroup v2
- **Arquitetura**: x86_64

### Dependências de Software

#### Obrigatórias
- **Compilador**: g++ com suporte a C++23
- **Make**: Para sistema de build
- **Bibliotecas padrão**: libc, libstdc++

#### Opcionais (para funcionalidades avançadas)
```bash
# Ferramentas de análise
sudo apt-get install valgrind      # Detecção de memory leaks
sudo apt-get install cppcheck      # Análise estática
sudo apt-get install doxygen       # Geração de documentação
sudo apt-get install linux-tools-generic  # perf para profiling


Permissões Necessárias
⚠️ Importante: Algumas funcionalidades requerem privilégios de root:

Criação e modificação de cgroups
Criação de namespaces
Movimentação de processos entre cgroups
Instruções de Compilação
Compilação Básica
# Clone o repositório
git clone <repository-url>
cd grupo14_RA3

# Compilação padrão
make

# Ou compilação com limpeza prévia
make clean && make


Opções de Compilação
# Build de debug (com símbolos de debug)
make debug

# Build de release (otimizado)
make release

# Compilar apenas os testes
make tests

# Verificar sintaxe e warnings
make check


Estrutura de Arquivos Gerada
grupo14_RA3/
├── monitor_app.out          # Aplicação principal
├── test_cpu.out            # Teste do monitor de CPU
├── test_memory.out          # Teste do monitor de memória
├── test_io.out             # Teste do monitor de I/O
└── build/                  # Arquivos intermediários
 └── obj/
     ├── main.o
     ├── cpu_monitor.o
     └── ...


Instruções de Uso
Execução Básica
# Executar todos os experimentos (requer sudo)
sudo ./monitor_app.out

# Executar em ambiente não-root (funcionalidade limitada)
./monitor_app.out


Exemplos de Uso
1. Monitoramento de Processo Específico
// Usar as funções da biblioteca em código próprio
#include "include/monitor.h"

process_stats_t stats;
int pid = 1234;
if (monitor_collect(pid, &stats) == 0) {
 printf("CPU: %.1f%%, RSS: %lu bytes\n", stats.cpu_percent, stats.rss);
}


2. Análise de Namespaces
// Usar o namespace analyzer
#include "include/namespace.h"

namespace_info_t ns_info;
if (analyzer::list_namespaces(pid, ns_info)) {
 for (auto& ns : ns_info.ns_map) {
     printf("%s -> %s\n", ns.first.c_str(), ns.second.c_str());
 }
}


3. Gerenciamento de Cgroups
// Exemplo de uso do cgroup manager
#include "include/cgroup.h"

std::string cg_path, err;
if (cgroup::create("meu_teste", cg_path, err)) {
 cgroup::set_cpu_limit(cg_path, 50, err);  // 50% de CPU
 cgroup::move_pid(cg_path, getpid(), err);
}


Executando Testes Individuais
# Testar componentes específicos
make run-tests

# Ou individualmente
./test_cpu.out
./test_memory.out
./test_io.out


Análise de Performance
# Verificar memory leaks
make valgrind

# Profiling de performance (requer root)
make profile

# Análise estática de código
make static-analysis


Interpretação de Resultados
Experimento 1 - Overhead de Monitoramento
Baseline: Tempo de execução sem monitoramento
Overhead: Impacto percentual do monitoramento
Latência de Sampling: Tempo para cada coleta de métricas


Experimento 2 - Namespaces
Isolation Table: Mostra quais namespaces diferem entre processos
Creation Overhead: Tempo necessário para criar cada tipo de namespace
Process Count: Quantos processos compartilham cada namespace


Experimento 3 - CPU Throttling
Measured vs Limit: Compara CPU% real com limite configurado
Deviation: Precisão do throttling em percentual
Throughput: Impacto na performance (iterações/segundo)


Experimento 4 - Memory Limits
Max Allocated: Quantidade máxima de memória alocada
Behavior: Como o sistema reagiu (OOM killer, malloc failures)
Failcnt: Número de falhas de alocação via cgroup


Experimento 5 - I/O Limits
Throughput: MB/s para leitura e escrita
Latency: Tempo médio por operação de I/O
Efficiency: Comparação com baseline sem limite

Fluxo de Execução
Inicialização: Verificação de permissões e criação de estruturas
Experimento 1: Medição de overhead de monitoramento
Experimento 2: Análise de isolamento via namespaces
Experimento 3: Testes de throttling de CPU
Experimento 4: Testes de limitação de memória
Experimento 5: Testes de limitação de I/O
Relatório: Geração de relatórios consolidados
Troubleshooting
Problemas Comuns
Erro de Permissão
# Solução: Executar como root
sudo ./monitor_app.out


Cgroups v1 vs v2
# Verificar versão do cgroup
mount | grep cgroup

# Se necessário, montar cgroup v2
sudo mount -t cgroup2 none /sys/fs/cgroup


Compilação Falha
# Verificar dependências
g++ --version  # Deve ser 11.0+
make --version

# Limpar e recompilar
make clean
make debug


Memory Leaks
# Executar com valgrind para debug
make valgrind

