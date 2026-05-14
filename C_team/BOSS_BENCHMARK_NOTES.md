# BOSS_Benchmark

Este B_team foi ajustado para servir como adversario de referencia: mais agressivo, mais vertical e mais disposto a pressionar depois de perder a bola.

## Alteracoes

- `src/start.sh`: nome padrao do time alterado para `BOSS_Benchmark`.
- `src/sample_field_evaluator.cpp`: avaliacao ofensiva refeita para valorizar proximidade do gol, faixa central, posse com atacantes, baixa pressao adversaria e chutes claros.
- `src/sample_player.cpp`: reativados os geradores de `ActGen_DirectPass` e `ActGen_SimpleDribble` para aumentar variedade de jogadas na action chain.
- `src/chain_action/action_chain_graph.cpp`: busca ofensiva aumentada de 4 para 5 acoes e limite de avaliacoes de 500 para 900.
- `src/bhv_basic_offensive_kick.cpp`: fallback ofensivo agora tenta `Body_ForceShoot` no ultimo terco quando ha chance real de finalizacao.
- `src/bhv_basic_move.cpp`: comportamento sem bola ganhou uma regra de counter-press quando o adversario esta com bola perto.
- `src/formations-dt/normal-formation.conf`, `offense-formation.conf`, `defense-formation.conf`: amostras centrais e de terco final foram ajustadas para empurrar meias/atacantes para zonas mais perigosas.

## Intencao Tatica

- Criar mais triangulos perto da area adversaria.
- Dar mais opcoes de passe vertical e diagonal.
- Recompensar jogadas que terminam em chute.
- Evitar jogadas presas na lateral.
- Pressionar mais rapido quando o rival esta kickable e a bola nao esta em zona defensiva profunda.

## Observacao

Essas mudancas priorizam forca ofensiva e dificuldade como adversario de treino. Depois de rodar partidas, o proximo ajuste natural e calibrar o equilibrio entre pressao e cobertura defensiva.
