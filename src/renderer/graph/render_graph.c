/**
 * @file render_graph.c
 * @brief Render graph implementation
 *
 * The render graph is how we describe a frame's worth of rendering work.
 * Instead of manually managing barriers and resource states, you declare
 * what resources each pass needs and the graph figures out the sync.
 *
 * This approach is increasingly common in modern engines (Frostbite, UE5,
 * etc.) because it makes the renderer way more maintainable. The downside
 * is it's more complex upfront. Worth it in the long run though.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

/* Placeholder - real implementation is substantial */
