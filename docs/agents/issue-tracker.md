# Issue tracker: GitHub

Los issues y specs de este repo son issues de GitHub (`javiercorrea/lineage-cliente`, privado). Todo se
hace con `gh`.

**Credenciales:** la cuenta activa de `gh` puede no ser `javiercorrea` (ver AGENTS.md, sección 5). Si `gh`
dice "Could not resolve to a Repository", correr los comandos con `GH_TOKEN=$(gh auth token -u javiercorrea)`.

## Convenciones

- **Crear:** `gh issue create --title "..." --body "..."` (heredoc para cuerpos de varias líneas). Sumar la
  etiqueta del área (`proto`, `render`, `ui`, `mundo`, `servidor`, `docs`).
- **Leer:** `gh issue view <n> --json number,title,body,labels,comments`.
- **Listar:** `gh issue list --state open --json number,title,body,labels,comments --jq '[.[] | {number, title, body, labels: [.labels[].name], comments: [.comments[].body]}]'`, con `--label` y `--state` según haga falta.
- **Sub-issue:** `gh issue create --parent <padre> ...` o `gh issue edit <padre> --add-sub-issue <hijo>` (`gh` 2.94+). Con `gh` viejo: `gh api --method POST repos/<owner>/<repo>/issues/<padre>/sub_issues -F sub_issue_id=<id-db-del-hijo>`. Sin sub-issues: `Part of #<padre>` al principio del cuerpo.
- **Comentar:** `gh issue comment <n> --body "..."`
- **Etiquetas:** `gh issue edit <n> --add-label "..."` / `--remove-label "..."`
- **Cerrar:** `gh issue close <n> --comment "..."`

## Pull requests como entrada de triage

**PRs as a request surface: no.** _(Poner `yes` si los PRs externos se tratan como pedidos; `/triage` lee esta línea.)_

Con `yes`, los PRs pasan por las mismas etiquetas con los equivalentes `gh pr` (`gh pr view <n> --comments`,
`gh pr diff <n>`, `gh pr edit --add-label`, `gh pr close`). Issues y PRs comparten numeración: resolver `#42`
con `gh pr view 42` y si falla `gh issue view 42`.

## Cuando un skill dice "publicar en el issue tracker"

Crear un issue de GitHub.

## Cuando un skill dice "traer el ticket"

Leerlo como en **Leer**.

## Wayfinding

Lo usa `/wayfinder`. El **mapa** es un issue; los **tickets** son issues hijos.

- **Mapa:** un issue con la etiqueta `wayfinder:map` (Notes / Decisions-so-far / Fog).
- **Ticket hijo:** sub-issue del mapa (o en una task list del mapa con `Part of #<mapa>`). Etiqueta `wayfinder:<tipo>` (`research`/`prototype`/`grilling`/`task`). Al tomarlo se asigna.
- **Bloqueos:** dependencias nativas: `gh api --method POST repos/<owner>/<repo>/issues/<hijo>/dependencies/blocked_by -F issue_id=<id-db-del-bloqueante>` (id de base: `gh api repos/<owner>/<repo>/issues/<n> --jq .id`). Sin dependencias: línea `Blocked by: #<n>, #<n>` al principio. Desbloqueado cuando todos los bloqueantes están cerrados.
- **Frontera:** hijos abiertos del mapa, sin bloqueantes abiertos y sin asignar; gana el primero en el orden del mapa.
- **Tomar:** `gh issue edit <n> --add-assignee @me`, la primera escritura de la sesión.
- **Resolver:** `gh issue comment <n> --body "<respuesta>"`, `gh issue close <n>`, y agregar un puntero (resumen + link) a Decisions-so-far del mapa.
