#include "rig-object.h"
#include "rig-interfaces.h"
#include "rig-transform-private.h"
#include "rig-property.h"
#include "components/rig-camera.h"

void *
rig_ref_countable_simple_ref (void *object)
{
  int *ref_count = rig_object_get_properties (object,
                                               RIG_INTERFACE_ID_REF_COUNTABLE);
  (*ref_count)++;
  return object;
}

void
rig_ref_countable_simple_unref (void *object)
{
  int *ref_count = rig_object_get_properties (object,
                                               RIG_INTERFACE_ID_REF_COUNTABLE);

  if (--(*ref_count) < 1)
    {
      RigRefCountableVTable *vtable =
        rig_object_get_vtable (object, RIG_INTERFACE_ID_REF_COUNTABLE);
      vtable->free (object);
    }
}

void *
rig_ref_countable_ref (void *object)
{
  RigObject *obj = object;
  const RigType *type = rig_object_get_type (obj);

  RigRefCountableVTable *vtable =
    type->interfaces[RIG_INTERFACE_ID_REF_COUNTABLE].vtable;

  return vtable->ref (obj);
}

void
rig_ref_countable_unref (void *object)
{
  RigObject *obj = object;
  const RigType *type = rig_object_get_type (obj);
  RigRefCountableVTable *vtable =
    type->interfaces[RIG_INTERFACE_ID_REF_COUNTABLE].vtable;

  vtable->unref (obj);
}

void
rig_graphable_init (RigObject *object)
{
  RigGraphableProps *props =
    rig_object_get_properties (object, RIG_INTERFACE_ID_GRAPHABLE);

  props->parent = NULL;
  props->children.head = NULL;
  props->children.tail = NULL;
  props->children.length = 0;
}

void
rig_graphable_add_child (RigObject *parent, RigObject *child)
{
  RigGraphableProps *parent_props =
    rig_object_get_properties (parent, RIG_INTERFACE_ID_GRAPHABLE);
  RigGraphableVTable *parent_vtable =
    rig_object_get_vtable (parent, RIG_INTERFACE_ID_GRAPHABLE);
  RigGraphableProps *child_props =
    rig_object_get_properties (child, RIG_INTERFACE_ID_GRAPHABLE);
  RigGraphableVTable *child_vtable =
    rig_object_get_vtable (child, RIG_INTERFACE_ID_GRAPHABLE);
  RigObject *old_parent = child_props->parent;

  if (old_parent)
    {
      RigGraphableVTable *old_parent_vtable =
        rig_object_get_vtable (old_parent, RIG_INTERFACE_ID_GRAPHABLE);
      if (old_parent_vtable->child_removed)
        old_parent_vtable->child_removed (old_parent, child);
    }

  child_props->parent = parent;
  if (child_vtable && child_vtable->parent_changed)
    child_vtable->parent_changed (child, old_parent, parent);

  if (parent_vtable && parent_vtable->child_added)
    parent_vtable->child_added (parent, child);

  /* XXX: maybe this should be deferred to parent_vtable->child_added ? */
  g_queue_push_tail (&parent_props->children, rig_ref_countable_ref (child));
}

void
rig_graphable_remove_child (RigObject *child)
{
  RigGraphableProps *child_props =
    rig_object_get_properties (child, RIG_INTERFACE_ID_GRAPHABLE);
  RigObject *parent = child_props->parent;
  RigGraphableProps *parent_props;

  if (!parent)
    return;

  parent_props = rig_object_get_properties (parent, RIG_INTERFACE_ID_GRAPHABLE);

  g_queue_remove (&parent_props->children, child);
  rig_ref_countable_unref (child);
  child_props->parent = NULL;
}

void
rig_graphable_remove_all_children (RigObject *parent)
{
  RigGraphableProps *parent_props =
    rig_object_get_properties (parent, RIG_INTERFACE_ID_GRAPHABLE);
  RigObject *child;

  while ((child = g_queue_pop_tail (&parent_props->children)))
    rig_graphable_remove_child (child);
}

RigObject *
rig_graphable_get_parent (RigObject *child)
{
  RigGraphableProps *child_props =
    rig_object_get_properties (child, RIG_INTERFACE_ID_GRAPHABLE);

  return child_props->parent;
}

static RigTraverseVisitFlags
_rig_graphable_traverse_breadth (RigObject *graphable,
                                 RigTraverseCallback callback,
                                 void *user_data)
{
  GQueue *queue = g_queue_new ();
  int dummy;
  int current_depth = 0;
  RigTraverseVisitFlags flags = 0;

  g_queue_push_tail (queue, graphable);
  g_queue_push_tail (queue, &dummy); /* use to delimit depth changes */

  while ((graphable = g_queue_pop_head (queue)))
    {
      if (graphable == &dummy)
        {
          current_depth++;
          g_queue_push_tail (queue, &dummy);
          continue;
        }

      flags = callback (graphable, current_depth, user_data);
      if (flags & RIG_TRAVERSE_VISIT_BREAK)
        break;
      else if (!(flags & RIG_TRAVERSE_VISIT_SKIP_CHILDREN))
        {
          RigGraphableProps *props =
            rig_object_get_properties (graphable, RIG_INTERFACE_ID_GRAPHABLE);
          GList *l;
          for (l = props->children.head; l; l = l->next)
            g_queue_push_tail (queue, l->data);
        }
    }

  g_queue_free (queue);

  return flags;
}

static RigTraverseVisitFlags
_rig_graphable_traverse_depth (RigObject *graphable,
                               RigTraverseCallback before_children_callback,
                               RigTraverseCallback after_children_callback,
                               int current_depth,
                               void *user_data)
{
  RigTraverseVisitFlags flags;

  flags = before_children_callback (graphable, current_depth, user_data);
  if (flags & RIG_TRAVERSE_VISIT_BREAK)
    return RIG_TRAVERSE_VISIT_BREAK;

  if (!(flags & RIG_TRAVERSE_VISIT_SKIP_CHILDREN))
    {
      RigGraphableProps *props =
        rig_object_get_properties (graphable, RIG_INTERFACE_ID_GRAPHABLE);
      GList *l;

      for (l = props->children.head; l; l = l->next)
        {
          flags = _rig_graphable_traverse_depth (l->data,
                                                 before_children_callback,
                                                 after_children_callback,
                                                 current_depth + 1,
                                                 user_data);

          if (flags & RIG_TRAVERSE_VISIT_BREAK)
            return RIG_TRAVERSE_VISIT_BREAK;
        }
    }

  if (after_children_callback)
    return after_children_callback (graphable, current_depth, user_data);
  else
    return RIG_TRAVERSE_VISIT_CONTINUE;
}

/* rig_graphable_traverse:
 * @graphable: The graphable object to start traversing the graph from
 * @flags: These flags may affect how the traversal is done
 * @before_children_callback: A function to call before visiting the
 *   children of the current object.
 * @after_children_callback: A function to call after visiting the
 *   children of the current object. (Ignored if
 *   %RIG_TRAVERSE_BREADTH_FIRST is passed to @flags.)
 * @user_data: The private data to pass to the callbacks
 *
 * Traverses the graph starting at the specified @root and descending
 * through all its children and its children's children.  For each
 * object traversed @before_children_callback and
 * @after_children_callback are called with the specified @user_data,
 * before and after visiting that object's children.
 *
 * The callbacks can return flags that affect the ongoing traversal
 * such as by skipping over an object's children or bailing out of
 * any further traversing.
 */
RigTraverseVisitFlags
rig_graphable_traverse (RigObject *root,
                        RigTraverseFlags flags,
                        RigTraverseCallback before_children_callback,
                        RigTraverseCallback after_children_callback,
                        void *user_data)
{
  if (flags & RIG_TRAVERSE_BREADTH_FIRST)
    return _rig_graphable_traverse_breadth (root,
                                            before_children_callback,
                                            user_data);
  else /* DEPTH_FIRST */
    return _rig_graphable_traverse_depth (root,
                                          before_children_callback,
                                          after_children_callback,
                                          0, /* start depth */
                                          user_data);
}

#if 0
static RigTraverseVisitFlags
_rig_graphable_paint_cb (RigObject *object,
                         int depth,
                         void *user_data)
{
  RigPaintContext *paint_ctx = user_data;
  RigPaintableVTable *vtable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_PAINTABLE);

  vtable->paint (object, paint_ctx);

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

void
rig_graphable_paint (RigObject *root,
                     RigCamera *camera)
{
  RigPaintContext paint_ctx;

  paint_ctx.camera = camera;

  rig_graphable_traverse (root,
                           RIG_TRAVERSE_DEPTH_FIRST,
                           _rig_graphable_paint_cb,
                           NULL, /* after callback */
                           &paint_ctx);
}
#endif

#if 0
RigCamera *
rig_graphable_find_camera (RigObject *object)
{
  do {
    RigGraphableProps *graphable_priv;

    if (rig_object_get_type (object) == &rig_camera_type)
      return RIG_CAMERA (object);

    graphable_priv =
      rig_object_get_properties (object, RIG_INTERFACE_ID_GRAPHABLE);

    object = graphable_priv->parent;

  } while (object);

  return NULL;
}
#endif

void
rig_graphable_apply_transform (RigObject *graphable,
                               CoglMatrix *transform_matrix)
{
  int depth = 0;
  RigObject **transform_nodes;
  RigObject *node = graphable;
  int i;

  do {
    RigGraphableProps *graphable_priv =
      rig_object_get_properties (node, RIG_INTERFACE_ID_GRAPHABLE);

    depth++;

    node = graphable_priv->parent;
  } while (node);

  transform_nodes = g_alloca (sizeof (RigObject *) * depth);

  node = graphable;
  i = 0;
  do {
    RigGraphableProps *graphable_priv;

    if (rig_object_is (node, RIG_INTERFACE_ID_TRANSFORMABLE))
      transform_nodes[i++] = node;

    graphable_priv =
      rig_object_get_properties (node, RIG_INTERFACE_ID_GRAPHABLE);
    node = graphable_priv->parent;
  } while (node);

  for (i--; i >= 0; i--)
    {
      const CoglMatrix *matrix = rig_transformable_get_matrix (transform_nodes[i]);
      cogl_matrix_multiply (transform_matrix, transform_matrix, matrix);
    }
}

void
rig_graphable_get_transform (RigObject *graphable,
                             CoglMatrix *transform)
{
  cogl_matrix_init_identity (transform);
  rig_graphable_apply_transform (graphable, transform);
}

void
rig_graphable_get_modelview (RigObject *graphable,
                             RigCamera *camera,
                             CoglMatrix *transform)
{
  const CoglMatrix *view = rig_camera_get_view_transform (camera);
  *transform = *view;
  rig_graphable_apply_transform (graphable, transform);
}

RigProperty *
rig_introspectable_lookup_property (RigObject *object,
                                    const char *name)
{
  RigIntrospectableVTable *introspectable_vtable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_INTROSPECTABLE);

  return introspectable_vtable->lookup_property (object, name);
}

void
rig_introspectable_foreach_property (RigObject *object,
                                     RigIntrospectablePropertyCallback callback,
                                     void *user_data)
{
  RigIntrospectableVTable *introspectable_vtable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_INTROSPECTABLE);

  introspectable_vtable->foreach_property (object, callback, user_data);
}

#if 0
void
rig_simple_introspectable_register_properties (RigObject *object,
                                               RigProperty *first_property)
{
  RigSimpleIntrospectableProps *props =
    rig_object_get_properties (object, RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE);
  RigPropertySpec *first_spec = first_property->spec;
  int n;

  for (n = 0; first_spec[n].name; n++)
    ;

  props->first_property = first_property;
  props->n_properties = n;
}
#endif

void
rig_simple_introspectable_init (RigObject *object,
                                RigPropertySpec *specs,
                                RigProperty *properties)
{
  RigSimpleIntrospectableProps *props =
    rig_object_get_properties (object, RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE);
  int n;

  for (n = 0; specs[n].name; n++)
    {
      rig_property_init (&properties[n],
                         &specs[n],
                         object);
    }

  props->first_property = properties;
  props->n_properties = n;
}


void
rig_simple_introspectable_destroy (RigObject *object)
{
  RigSimpleIntrospectableProps *props =
    rig_object_get_properties (object, RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE);
  RigProperty *properties = props->first_property;
  int i;

  for (i = 0; i < props->n_properties; i++)
    rig_property_destroy (&properties[i]);
}

RigProperty *
rig_simple_introspectable_lookup_property (RigObject *object,
                                           const char *name)
{
  RigSimpleIntrospectableProps *priv =
    rig_object_get_properties (object, RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE);
  int i;

  for (i = 0; i < priv->n_properties; i++)
    {
      RigProperty *property = priv->first_property + i;
      if (strcmp (property->spec->name, name) == 0)
        return property;
    }

  return NULL;
}

void
rig_simple_introspectable_foreach_property (RigObject *object,
                                            RigIntrospectablePropertyCallback callback,
                                            void *user_data)
{
  RigSimpleIntrospectableProps *priv =
    rig_object_get_properties (object, RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE);
  int i;

  for (i = 0; i < priv->n_properties; i++)
    {
      RigProperty *property = priv->first_property + i;
      callback (property, user_data);
    }
}

const CoglMatrix *
rig_transformable_get_matrix (RigObject *object)
{
  RigTransformableVTable *transformable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_TRANSFORMABLE);

  return transformable->get_matrix (object);
}

void
rig_sizable_set_size (RigObject *object,
                      float width,
                      float height)
{
  RigSizableVTable *sizable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_SIZABLE);

  sizable->set_size (object,
                     width,
                     height);
}

void
rig_sizable_get_size (void *object,
                      float *width,
                      float *height)
{
  RigSizableVTable *sizable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_SIZABLE);

  sizable->get_size (object,
                     width,
                     height);
}

void
rig_sizable_get_preferred_width (void *object,
                                 float for_height,
                                 float *min_width_p,
                                 float *natural_width_p)
{
  RigSizableVTable *sizable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_SIZABLE);

  sizable->get_preferred_width (object,
                                for_height,
                                min_width_p,
                                natural_width_p);
}

void
rig_sizable_get_preferred_height (void *object,
                                  float for_width,
                                  float *min_height_p,
                                  float *natural_height_p)
{
  RigSizableVTable *sizable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_SIZABLE);

  sizable->get_preferred_height (object,
                                 for_width,
                                 min_height_p,
                                 natural_height_p);
}

CoglPrimitive *
rig_primable_get_primitive (RigObject *object)
{
  RigPrimableVTable *primable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_PRIMABLE);

  return primable->get_primitive (object);
}

void *
rig_pickable_get_vertex_data (RigObject *object,
                              size_t *stride,
                              int *n_vertices)
{
  RigPickableVTable *pickable =
    rig_object_get_vtable (object, RIG_INTERFACE_ID_PICKABLE);

  return pickable->get_vertex_data (object, stride, n_vertices);
}

