
#ifndef __brasero_marshal_MARSHAL_H__
#define __brasero_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:DOUBLE,LONG (./brasero-marshal.list:1) */
extern void brasero_marshal_VOID__DOUBLE_LONG (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

/* INT:STRING,BOOLEAN (./brasero-marshal.list:2) */
extern void brasero_marshal_INT__STRING_BOOLEAN (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);

/* INT:VOID (./brasero-marshal.list:3) */
extern void brasero_marshal_INT__VOID (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);

/* INT:INT (./brasero-marshal.list:4) */
extern void brasero_marshal_INT__INT (GClosure     *closure,
                                      GValue       *return_value,
                                      guint         n_param_values,
                                      const GValue *param_values,
                                      gpointer      invocation_hint,
                                      gpointer      marshal_data);

/* INT:INT,INT (./brasero-marshal.list:5) */
extern void brasero_marshal_INT__INT_INT (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

/* INT:BOOLEAN,BOOLEAN,BOOLEAN (./brasero-marshal.list:6) */
extern void brasero_marshal_INT__BOOLEAN_BOOLEAN_BOOLEAN (GClosure     *closure,
                                                          GValue       *return_value,
                                                          guint         n_param_values,
                                                          const GValue *param_values,
                                                          gpointer      invocation_hint,
                                                          gpointer      marshal_data);

/* VOID:DOUBLE,STRING (./brasero-marshal.list:7) */
extern void brasero_marshal_VOID__DOUBLE_STRING (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);

/* VOID:INT64 (./brasero-marshal.list:8) */
extern void brasero_marshal_VOID__INT64 (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* VOID:INT,STRING (./brasero-marshal.list:9) */
extern void brasero_marshal_VOID__INT_STRING (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:DOUBLE,DOUBLE,LONG (./brasero-marshal.list:10) */
extern void brasero_marshal_VOID__DOUBLE_DOUBLE_LONG (GClosure     *closure,
                                                      GValue       *return_value,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data);

G_END_DECLS

#endif /* __brasero_marshal_MARSHAL_H__ */

