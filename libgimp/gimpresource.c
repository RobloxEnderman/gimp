/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gimp.h"

#include "libgimpbase/gimpwire.h" /* FIXME kill this include */

#include "gimpplugin-private.h"
#include "gimpprocedure-private.h"


/* GimpResource: base class for resources.
 *
 * Known subclasses are Font, Brush, Pattern, Palette, Gradient.
 * FUTURE: ? Dynamics, ToolPreset, ColorProfile, Color, ColorScale
 * (GimpParasite is NOT.)
 *
 * A *resource* is data that GIMP loads at runtime startup,
 * where the data is used by drawing tools.
 * The GimpContext holds a user's current choice of resources.
 * The GIMP core has the *resource* data.
 *
 * A resource has-a identifier.
 * Currently the identifier is a string, sometimes called a name.
 * The identifier is unique among instances(resource datas) loaded into GIMP.
 *
 * A user can change the set of resources installed with GIMP,
 * and edit or create new resources meaning datasets.
 * A user may attempt to install two different resources having the same name.
 * A user may uninstall a resource while there are still references to it in settings.
 *
 * FUTURE: the identifier is world unique, a UUID or an integer incremented by core.
 * Uniqueness is enforced by core.
 *
 * A GimpResource's identifier is serialized as a setting, i.e. a user's choice,
 * A serialization of a GimpResource is *not* the serialization of the
 * resource's underlying data e.g. not the pixels of a brush.
 *
 * The GimpResource identifier is opaque: you should only pass it around.
 * You should not assume it has any human readable meaning (although it does now.)
 * You should not assume that the "id" is a string.
 * The Resource class encapsulates the ID so in the future, the type may change.
 * PDB procedures that pass a string ID for a resource are obsolete.
 *
 * Usually a plugin lets a user choose a resource interactively
 * then sets it into a temporary context to affect subsequent operations.
 *
 * The Gimp architecture for plugins uses remote procedures,
 * and identically named classes on each side of the wire.
 * A GimpBrush class in core is not the same class as GimpBrush in libgimp.
 * a GimpResource on the libgimp side is a proxy.
 * There is no GimpResource class in core.
 *
 * One use of GimpResource and its subclasses is as a held type of
 * GParamSpecObject, used to declare the parameters of a PDB
 * procedure.  A GimpResource just holds the id as a way to identify a
 * *resource* in calls to PDB procedures that ultimately access the
 * core instance of the resource.
 *
 * A GimpResource that has been serialized in a GimpConfig refers to a *resource*
 * that might not still exist in core in the set of loaded resources (files.)
 *
 * A GimpResource:
 *  - furnishes its ID as a property
 *  - serializes/deserializes itself (implements GimpConfigInterface)
 *
 * Some subclasses e.g. GimpFont are pure types.
 * That is, inheriting all its properties and methods from GimpResource.
 * Such a pure type exists just to distinguish (by the name of its type)
 * from other subclasses of GimpResource.
 *
 * Some subclasses have methods for getting/setting attributes of a resource.
 * Some subclasses have methods for creating, duplicating, and deleting resources.
 * Most methods are defined in the PDB (in .pdb files.)
 *
 * Internally, you may need to create a proxy object. Use:
 *    brush = g_object_new (GIMP_TYPE_BRUSH, NULL);
 *    g_object_set (GIMP_RESOURCE(brush), "id", "foo name", NULL);
 * This does NOT create the resource's data in core, only a reference to it.
 * When there is no underlying resource of that id (name) in core,
 * the brush is invalid.
 */

enum
{
  PROP_0,
  PROP_ID,
  N_PROPS
};


typedef struct _GimpResourcePrivate
{
  gint id;
} GimpResourcePrivate;


static void     gimp_resource_set_property       (GObject      *object,
                                                  guint         property_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec);
static void     gimp_resource_get_property       (GObject      *object,
                                                  guint         property_id,
                                                  GValue       *value,
                                                  GParamSpec   *pspec);


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GimpResource, gimp_resource, G_TYPE_OBJECT)

#define parent_class gimp_resource_parent_class

static GParamSpec *props[N_PROPS] = { NULL, };


static void
gimp_resource_class_init (GimpResourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gimp_resource_set_property;
  object_class->get_property = gimp_resource_get_property;

  props[PROP_ID] =
    g_param_spec_int ("id",
                      "The id",
                      "The id for internal use",
                      0, G_MAXINT32, 0,
                      GIMP_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
gimp_resource_init (GimpResource *resource)
{
}

static void
gimp_resource_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GimpResource        *resource = GIMP_RESOURCE (object);
  GimpResourcePrivate *priv     = gimp_resource_get_instance_private (resource);

  switch (property_id)
    {
    case PROP_ID:
      priv->id = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_resource_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GimpResource        *resource = GIMP_RESOURCE (object);
  GimpResourcePrivate *priv     = gimp_resource_get_instance_private (resource);

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_int (value, priv->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/**
 * gimp_resource_get_id:
 * @resource: The resource.
 *
 * Returns: the resource ID.
 *
 * Since: 3.0
 **/
gint32
gimp_resource_get_id (GimpResource *resource)
{
  if (resource)
    {
      GimpResourcePrivate *priv = gimp_resource_get_instance_private (resource);

      return priv->id;
    }
  else
    {
      return -1;
    }
}

/**
 * gimp_resource_get_by_id:
 * @resource_id: The resource id.
 *
 * Returns a #GimpResource representing @resource_id. Since #GimpResource is an
 * abstract class, the real object type will actually be the proper
 * subclass.
 *
 * Returns: (nullable) (transfer none): a #GimpResource for @resource_id or
 *          %NULL if @resource_id does not represent a valid resource.
 *          The object belongs to libgimp and you must not modify
 *          or unref it.
 *
 * Since: 3.0
 **/
GimpResource *
gimp_resource_get_by_id (gint32 resource_id)
{
  if (resource_id > 0)
    {
      GimpPlugIn    *plug_in   = gimp_get_plug_in ();
      GimpProcedure *procedure = _gimp_plug_in_get_procedure (plug_in);

      return _gimp_procedure_get_resource (procedure, resource_id);
    }

  return NULL;
}

/**
 * gimp_resource_get_by_name:
 * @resource_type: The #GType of the resource.
 * @resource_name: The name of the resource.
 *
 * Returns the resource with the given @resource_type and
 * @resource_name.
 *
 * Returns: (transfer full): The resource.
 *
 * Since: 3.0
 **/
GimpResource *
gimp_resource_get_by_name (GType        resource_type,
                           const gchar *resource_name)
{
  g_return_val_if_fail (g_type_is_a (resource_type, GIMP_TYPE_RESOURCE), NULL);

  if (resource_name == NULL)
    return NULL;

  if (g_type_is_a (resource_type, GIMP_TYPE_BRUSH))
    {
      return (GimpResource *) gimp_brush_get_by_name (resource_name);
    }
  else if (g_type_is_a (resource_type, GIMP_TYPE_PATTERN))
    {
      return (GimpResource *) gimp_pattern_get_by_name (resource_name);
    }
  else if (g_type_is_a (resource_type, GIMP_TYPE_GRADIENT))
    {
      return (GimpResource *) gimp_gradient_get_by_name (resource_name);
    }
  else if (g_type_is_a (resource_type, GIMP_TYPE_PALETTE))
    {
      return (GimpResource *) gimp_palette_get_by_name (resource_name);
    }
  else if (g_type_is_a (resource_type, GIMP_TYPE_FONT))
    {
      return (GimpResource *) gimp_font_get_by_name (resource_name);
    }

  g_return_val_if_reached (NULL);
}

/**
 * gimp_resource_is_valid:
 * @resource: The resource to check.
 *
 * Returns TRUE if the resource is valid.
 *
 * This procedure checks if the given resource is valid and refers to an
 * existing resource.
 *
 * Returns: Whether the resource is valid.
 *
 * Since: 3.0
 **/
gboolean
gimp_resource_is_valid (GimpResource *resource)
{
  return gimp_resource_id_is_valid (gimp_resource_get_id (resource));
}

/**
 * gimp_resource_is_brush:
 * @resource: The resource.
 *
 * Returns whether the resource is a brush.
 *
 * This procedure returns TRUE if the specified resource is a brush.
 *
 * Returns: TRUE if the resource is a brush, FALSE otherwise.
 *
 * Since: 3.0
 **/
gboolean
gimp_resource_is_brush (GimpResource *resource)
{
  return gimp_resource_id_is_brush (gimp_resource_get_id (resource));
}

/**
 * gimp_resource_is_pattern:
 * @resource: The resource.
 *
 * Returns whether the resource is a pattern.
 *
 * This procedure returns TRUE if the specified resource is a pattern.
 *
 * Returns: TRUE if the resource is a pattern, FALSE otherwise.
 *
 * Since: 3.0
 **/
gboolean
gimp_resource_is_pattern (GimpResource *resource)
{
  return gimp_resource_id_is_pattern (gimp_resource_get_id (resource));
}

/**
 * gimp_resource_is_gradient:
 * @resource: The resource.
 *
 * Returns whether the resource is a gradient.
 *
 * This procedure returns TRUE if the specified resource is a gradient.
 *
 * Returns: TRUE if the resource is a gradient, FALSE otherwise.
 *
 * Since: 3.0
 **/
gboolean
gimp_resource_is_gradient (GimpResource *resource)
{
  return gimp_resource_id_is_gradient (gimp_resource_get_id (resource));
}

/**
 * gimp_resource_is_palette:
 * @resource: The resource.
 *
 * Returns whether the resource is a palette.
 *
 * This procedure returns TRUE if the specified resource is a palette.
 *
 * Returns: TRUE if the resource is a palette, FALSE otherwise.
 *
 * Since: 3.0
 **/
gboolean
gimp_resource_is_palette (GimpResource *resource)
{
  return gimp_resource_id_is_palette (gimp_resource_get_id (resource));
}

/**
 * gimp_resource_is_font:
 * @resource: The resource.
 *
 * Returns whether the resource is a font.
 *
 * This procedure returns TRUE if the specified resource is a font.
 *
 * Returns: TRUE if the resource is a font, FALSE otherwise.
 *
 * Since: 3.0
 **/
gboolean
gimp_resource_is_font (GimpResource *resource)
{
  return gimp_resource_id_is_font (gimp_resource_get_id (resource));
}


struct _GimpBrush
{
  GimpResource parent_instance;
};

G_DEFINE_TYPE (GimpBrush, gimp_brush, GIMP_TYPE_RESOURCE);

static void gimp_brush_class_init (GimpBrushClass *klass) {}
static void gimp_brush_init       (GimpBrush       *self) {}


struct _GimpPattern
{
  GimpResource parent_instance;
};

G_DEFINE_TYPE (GimpPattern, gimp_pattern, GIMP_TYPE_RESOURCE);

static void gimp_pattern_class_init (GimpPatternClass *klass) {}
static void gimp_pattern_init       (GimpPattern       *self) {}


struct _GimpGradient
{
  GimpResource parent_instance;
};

G_DEFINE_TYPE (GimpGradient, gimp_gradient, GIMP_TYPE_RESOURCE);

static void gimp_gradient_class_init (GimpGradientClass *klass) {}
static void gimp_gradient_init       (GimpGradient       *self) {}


struct _GimpPalette
{
  GimpResource parent_instance;
};

G_DEFINE_TYPE (GimpPalette, gimp_palette, GIMP_TYPE_RESOURCE);

static void gimp_palette_class_init (GimpPaletteClass *klass) {}
static void gimp_palette_init       (GimpPalette       *self) {}


struct _GimpFont
{
  GimpResource parent_instance;
};

G_DEFINE_TYPE (GimpFont, gimp_font, GIMP_TYPE_RESOURCE);

static void gimp_font_class_init (GimpFontClass *klass) {}
static void gimp_font_init       (GimpFont       *self) {}
